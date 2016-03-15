/**
 * This file is part of libstreamswtich, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/**
 * stsw_aac_mux_parser.cc
 *      AacMuxParser class implementation file, define methods of the AacMuxParser 
 * class. 
 *      AacMuxParser is sub class of StreamMuxParser, which is reponsible to 
 * handle the aac codec
 * 
 * author: OpenSight Team
 * date: 2016-2-3
**/ 

#include "stsw_aac_mux_parser.h"
#include "../stsw_ffmpeg_muxer.h"
#include <stdint.h>
#include <strings.h>
#include <stream_switch.h>

#include "../stsw_log.h"
#include "../stsw_ffmpeg_sender_global.h"

extern "C"{

#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>    
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avassert.h>
#include <libavutil/avstring.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
}

enum AVCodecID CodecIdFromName(std::string codec_name);




/////////////////////////////////////////////////////////////
//utils function

static int InitOutputFrame(AVFrame *frame,
                           AVCodecContext *output_codec_context,
                           int frame_size)
{
    using namespace stream_switch; 
    int error;

    av_frame_unref(frame);
    /**
     * Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity.
     */
    frame->nb_samples     = frame_size;
    frame->channel_layout = output_codec_context->channel_layout;
    frame->format         = output_codec_context->sample_fmt;
    frame->sample_rate    = output_codec_context->sample_rate;

    /**
     * Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified.
     */
    if ((error = av_frame_get_buffer(frame, 0)) < 0) {
        STDERR_LOG(LOG_LEVEL_ERR, 
            "Could allocate output frame samples (error '%s')\n", 
            av_err2str(error));             
        return FFMPEG_SENDER_ERR_GENERAL;  
    }

    return 0;
}
/**
 * Initialize a temporary storage for the specified number of audio samples.
 * The conversion requires temporary storage due to the different format.
 * The number of audio samples to be allocated is specified in frame_size.
 */
static int init_converted_samples(uint8_t ***converted_input_samples,
                                  AVCodecContext *output_codec_context,
                                  int frame_size)
{
    using namespace stream_switch; 
    int error;

    /**
     * Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples = (uint8_t **)calloc(output_codec_context->channels,
                                            sizeof(**converted_input_samples)))) {
        STDERR_LOG(LOG_LEVEL_ERR, "Could not allocate converted input sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /**
     * Allocate memory for the samples of all channels in one consecutive
     * block for convenience.
     */
    if ((error = av_samples_alloc(*converted_input_samples, NULL,
                                  output_codec_context->channels,
                                  frame_size,
                                  output_codec_context->sample_fmt, 0)) < 0) {
        STDERR_LOG(LOG_LEVEL_ERR, 
                "Could not allocate converted input samples (error '%s')\n",
                av_err2str(error));
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}




/////////////////////////////////////////////////////////////
//AacMuxParser methods


AacMuxParser::AacMuxParser()
:StreamMuxParser(), transcoding_(false), in_codec_context_(NULL), 
out_codec_context_(NULL), resample_context_(NULL), fifo_(NULL), 
fifo_pts_(AV_NOPTS_VALUE)
{
    input_frame_ = av_frame_alloc();
    output_frame_ = av_frame_alloc();    
}
AacMuxParser::~AacMuxParser()
{
    av_frame_free(&input_frame_);
    av_frame_free(&output_frame_);    
}

int AacMuxParser::Init(FFmpegMuxer * muxer, 
                     const std::string &codec_name, 
                     const stream_switch::SubStreamMetadata &sub_metadata, 
                     AVFormatContext *fmt_ctx)
{
    using namespace stream_switch; 
    int ret = 0;
    if(is_init_){
        return 0;
    }
    
    if(strcasecmp(codec_name.c_str(),sub_metadata.codec_name.c_str()) == 0){
        transcoding_ = false;    
        return StreamMuxParser::Init(muxer, codec_name, sub_metadata, fmt_ctx);        
    }    
        
    //need transcoding
    if(strcasecmp(codec_name.c_str(), "aac") != 0){
        STDERR_LOG(LOG_LEVEL_ERR, 
            "This parser does not support transcoding from codec %s\n", 
            codec_name.c_str());             
        return FFMPEG_SENDER_ERR_NOT_SUPPORT;         
    }


    // init the decoding codec context
    ret = InitInputContext(sub_metadata);
    if(ret){
        goto error_1;
    }
    //init the encoding codec context
    ret = InitOutputContext(muxer, codec_name, fmt_ctx);
    if(ret){
        goto error_2;
    }
                
    ret = InitResampler();
    if(ret){
        goto error_3;
    }
    
    ret = InitAudioFifo();
    if(ret){
        goto error_4;
    }   
    
    ret = InitOutputFrame(output_frame_, 
                          out_codec_context_, 
                          out_codec_context_->frame_size);
    if(ret){
        goto error_5;
    }                            
    
    transcoding_ = true; 
    
    muxer_ = muxer;
    fmt_ctx_ = fmt_ctx;
    gop_started_ = false;
    codec_name_ = codec_name;
    fifo_pts_ = 0;

    is_init_ = true; 
    
#define DUMP_INPUT_CONTEXT
#ifdef DUMP_INPUT_CONTEXT
    char buf[256];
    avcodec_string(buf, sizeof(buf), in_codec_context_, 0);    
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO,  
            "AacMuxParser::Init(): Transcoding from the codec context (%s) to %s\n",
            buf, codec_name.c_str());    
#endif    
    
    return 0;

error_6:
    av_frame_unref(output_frame_);

error_5:
    if (fifo_){
        av_audio_fifo_free(fifo_);
        fifo_ = NULL;        
    }

error_4:
    if(resample_context_ != NULL){
        swr_free(&resample_context_);
    }

error_3:
    stream_ = NULL;
    
    if(out_codec_context_ != NULL){
        avcodec_close(out_codec_context_);
        out_codec_context_ = NULL;
    }


error_2:
    if(in_codec_context_ != NULL){
        avcodec_close(in_codec_context_);
        avcodec_free_context(&in_codec_context_);
    }
    
error_1:
    return ret;
}
    
void AacMuxParser::Uninit()
{
    if(!transcoding_){
        StreamMuxParser::Uninit();
        return;
    }

    if(!is_init_){
        return;
    }
    is_init_ = false;

    muxer_ = NULL;
    stream_ = NULL;
    fmt_ctx_ = NULL;
    transcoding_ = false;
    fifo_pts_ = 0;
    
    av_frame_unref(output_frame_);
    
    if (fifo_){
        av_audio_fifo_free(fifo_);
        fifo_ = NULL;        
    }

    if(resample_context_ != NULL){
        swr_free(&resample_context_);
    }

    if(out_codec_context_ != NULL){
        avcodec_close(out_codec_context_);
        out_codec_context_ = NULL;
    }

    if(in_codec_context_ != NULL){
        avcodec_close(in_codec_context_);
        avcodec_free_context(&in_codec_context_);
    }
    
}
void AacMuxParser::Flush()
{
    if(!transcoding_){
        StreamMuxParser::Flush();
        return;
    }
    FlushInternal();
}
    
int AacMuxParser::Parse(const stream_switch::MediaFrameInfo *frame_info, 
                      const char * frame_data,
                      size_t frame_size,
                      struct timeval *base_timestamp,
                      AVPacket *opkt)
{
    using namespace stream_switch; 
    if(!transcoding_){
        return StreamMuxParser::Parse(frame_info, frame_data, frame_size, 
                                      base_timestamp, opkt);
    }   
    
    int ret = 0;
    const int output_frame_size = out_codec_context_->frame_size;
    
    if(frame_info != NULL){
        int error;        
        AVPacket input_pkt = { 0 };
        av_init_packet(&input_pkt);
        int data_present = 0;
        
        //printf("get here %d\n", __LINE__);
        
/*       
        printf("ts before parse: %lld.%d, size: %d\n", 
               (long long)frame_info->timestamp.tv_sec, 
               (long)frame_info->timestamp.tv_usec, 
               (int)frame_size);
*/        
        
        //1. get AVPacket from frame_info
        ret = InitInputPacket(frame_info, frame_data, frame_size, 
                              base_timestamp, &input_pkt);
        if(ret <= 0){            
            return ret;
        }
//       printf("PTS after parse: %lld, size: %d\n", (long long)input_pkt.pts, (int)input_pkt.size);
//        printf("get here %d\n", __LINE__);
        
        //2. decode packet
        /**
         * Decode the audio frame stored in the temporary packet.
         * The input audio stream decoder is used to do this.
         */
        if ((error = avcodec_decode_audio4(in_codec_context_, input_frame_,
                                           &data_present, &input_pkt)) < 0) {
            STDERR_LOG(LOG_LEVEL_ERR, "Could not decode frame (error '%s')\n",
                       av_err2str(error)); 
            av_free_packet(&input_pkt);
            return FFMPEG_SENDER_ERR_CODEC;
        }
        av_free_packet(&input_pkt);
        
//        printf("get here %d\n", __LINE__);
        
        if (data_present) {
//            printf("get here %d\n", __LINE__);
            uint8_t **converted_input_samples = NULL;
            
            //3. check pts consistent and flush
            int64_t pts = 0;
            if(input_frame_->pts != AV_NOPTS_VALUE){
                pts = input_frame_->pts;
            }else if(input_frame_->pkt_pts != AV_NOPTS_VALUE){
                pts = input_frame_->pkt_pts;
            }else if(input_frame_->pkt_dts != AV_NOPTS_VALUE){
                pts = input_frame_->pkt_dts;
            }
            
/*
            printf("Frame pts/size: %lld/%d, fifo pts/size: %lld/%d\n", 
                   (long long)pts, 
                   (int)input_frame_->nb_samples,
                   (long long)fifo_pts_,
                   (int)av_audio_fifo_size(fifo_));
*/          

            
            if(CheckAudioFifoPts(pts, out_codec_context_->sample_rate)){
                if(fifo_pts_ != 0 && pts < fifo_pts_){
                    //this audio packet pts is too early, which would destroy pts monotonically increasing
                    STDERR_LOG(LOG_LEVEL_WARNING, 
                        "Audio packet->pts(%lld) is earier than fifo_pts(%lld), drop\n",
                        (long long)pts, (long long)fifo_pts_);
                    return 0;
                }else{
                    //audio pts is acceptable, reset the fifo
                    STDERR_LOG(LOG_LEVEL_WARNING, "Audio fifo pts inconsistent, cleanup Fifo\n");
                    CleanupAudioFifo();                    
                }
            }            
            
            //4. resample
            
            /** Initialize the temporary storage for the converted input samples. */
            if (init_converted_samples(&converted_input_samples, out_codec_context_,
                                       input_frame_->nb_samples)){
                return FFMPEG_SENDER_ERR_GENERAL;                                           
            }
            
            /** Convert the samples using the resampler. */
            if ((error = swr_convert(resample_context_,
                                     converted_input_samples, 
                                     input_frame_->nb_samples,
                                     (const uint8_t**)input_frame_->extended_data, 
                                     input_frame_->nb_samples)) < 0) {
                if (converted_input_samples) {
                    av_freep(&converted_input_samples[0]);
                    free(converted_input_samples);
                }
                STDERR_LOG(LOG_LEVEL_ERR, "Could not convert input samples (error '%s')\n",
                        av_err2str(error));
                return FFMPEG_SENDER_ERR_CODEC;  
            }                
            
            //5. write to fifo
            if(WriteAudioFifo((void **)converted_input_samples, input_frame_->nb_samples, 
                              pts) < input_frame_->nb_samples){
                if (converted_input_samples) {
                    av_freep(&converted_input_samples[0]);
                    free(converted_input_samples);
                }
                STDERR_LOG(LOG_LEVEL_ERR, "Could not write data to FIFO\n");
                return FFMPEG_SENDER_ERR_GENERAL;                                    
            }

        
            if (converted_input_samples) {
                av_freep(&converted_input_samples[0]);
                free(converted_input_samples);
            } 
        }//if (data_present) 
        
    }//if(frame_info != NULL)
    
//    printf("get here %d\n", __LINE__);
    while (av_audio_fifo_size(fifo_) >= output_frame_size) {  
        
//        printf("get here %d\n", __LINE__);
        int data_present = 0;
        int error;  
        
        //5. get frame from fifo
        if (ReadAudioFifo((void **)output_frame_->data, 
                          output_frame_size, 
                          &(output_frame_->pts)) < output_frame_size) {
            STDERR_LOG(LOG_LEVEL_ERR, "Could not read data from FIFO\n"); 
            return FFMPEG_SENDER_ERR_GENERAL;
        }        
/*
            printf("output Frame pts/size: %lld/%d\n", 
                   (long long)output_frame_->pts, 
                   (int)output_frame_size);
*/        
        //6. encode frame         
   
        /**
        * Encode the audio frame and store it in the output packet.
        * The output audio stream encoder is used to do this.
        */
        if ((error = avcodec_encode_audio2(out_codec_context_, opkt,
                                           output_frame_, &data_present)) < 0) {
            av_free_packet(opkt);
            STDERR_LOG(LOG_LEVEL_ERR, 
                "Could not encode frame (error '%s')\n", 
                av_err2str(error));             
            return FFMPEG_SENDER_ERR_CODEC; 
        }
        
        if (data_present) {
             /* return the packet to caller  */   
             //printf("get here %d\n", __LINE__);
             /* prepare packet for muxing */
/*        
            printf("output packet pts/duration: %lld/%d\n", 
                   (long long)opkt->pts, 
                   (int)opkt->duration);      
*/ 
             opkt->stream_index = stream_->index;
             av_packet_rescale_ts(opkt,
                                  out_codec_context_->time_base,
                                  stream_->time_base);            
             return 1;
        }
        
    }
            
    return 0; //no packet to output 
                          
}


int AacMuxParser::InitInputContext(const stream_switch::SubStreamMetadata &sub_metadata)
{
    using namespace stream_switch;
    AVCodec *input_codec = NULL;
    int i;
    enum AVCodecID codec_id = AV_CODEC_ID_NONE;    
    int ret = 0;
    
    codec_id = CodecIdFromName(sub_metadata.codec_name);
    if(codec_id == AV_CODEC_ID_NONE){
        STDERR_LOG(LOG_LEVEL_ERR, "decoder for %s not support\n", 
                   sub_metadata.codec_name.c_str());             
        return FFMPEG_SENDER_ERR_NOT_SUPPORT;
    }    
    input_codec = avcodec_find_decoder(codec_id);
    if (input_codec == NULL) {
        STDERR_LOG(LOG_LEVEL_ERR, "Could not find decoder for '%s'\n",
                sub_metadata.codec_name.c_str());
        return FFMPEG_SENDER_ERR_NOT_SUPPORT;
    }  
    in_codec_context_ = avcodec_alloc_context3(input_codec);
    if (!in_codec_context_) {
       STDERR_LOG(LOG_LEVEL_ERR, "Could not allocate audio codec context for '%s'\n",
                input_codec->name);        
        return FFMPEG_SENDER_ERR_GENERAL;
    }    
    if(sub_metadata.media_param.audio.samples_per_second != 0){
        in_codec_context_->sample_rate = 
                sub_metadata.media_param.audio.samples_per_second;
    }else{
        in_codec_context_->sample_rate = 8000; //the lowest rate
        if (input_codec->supported_samplerates) {
            in_codec_context_->sample_rate = input_codec->supported_samplerates[0];
            for (i = 0; input_codec->supported_samplerates[i]; i++) {
                if (input_codec->supported_samplerates[i] == 8000)
                    in_codec_context_->sample_rate = 8000;
            }
        }
    }       
    if(sub_metadata.media_param.audio.channels != 0){
        in_codec_context_->channels = sub_metadata.media_param.audio.channels;
        in_codec_context_->channel_layout = 
            av_get_default_channel_layout(in_codec_context_->channels);
    }else{
        in_codec_context_->channel_layout = AV_CH_LAYOUT_MONO;
        if (input_codec->channel_layouts) {
            in_codec_context_->channel_layout = input_codec->channel_layouts[0];
            for (i = 0; input_codec->channel_layouts[i]; i++) {
                if (input_codec->channel_layouts[i] == AV_CH_LAYOUT_MONO)
                    in_codec_context_->channel_layout = AV_CH_LAYOUT_MONO;
            }
        }
        in_codec_context_->channels  = 
            av_get_channel_layout_nb_channels(in_codec_context_->channel_layout);                
    }  
    
    if(sub_metadata.extra_data.size() != 0){    
        in_codec_context_->extradata_size = sub_metadata.extra_data.size();
        in_codec_context_->extradata = (uint8_t *)av_mallocz(in_codec_context_->extradata_size);
        memcpy(in_codec_context_->extradata, sub_metadata.extra_data.data(), 
               in_codec_context_->extradata_size);                               
    }     
    
    ret = avcodec_open2(in_codec_context_, input_codec, NULL);
    if(ret){
        STDERR_LOG(LOG_LEVEL_ERR, "Could not open input code (%s) context: %s\n",
                sub_metadata.codec_name.c_str(), av_err2str(ret));
        avcodec_free_context(&in_codec_context_);
        return FFMPEG_SENDER_ERR_GENERAL;                
    }    
    
    return 0;
}
int AacMuxParser::InitOutputContext(FFmpegMuxer * muxer, 
                                    const std::string &codec_name, 
                                    AVFormatContext *fmt_ctx)
{
    using namespace stream_switch; 
    AVCodecContext *c = NULL;
    AVCodec *codec = NULL;
    AVStream * stream = NULL;    
    enum AVCodecID codec_id = AV_CODEC_ID_NONE;  
    int ret = 0;
    
    codec_id = CodecIdFromName(codec_name.c_str());
    if(codec_id == AV_CODEC_ID_NONE){
        STDERR_LOG(LOG_LEVEL_ERR, "encoder for %s not support\n", 
                   codec_name.c_str());             
        return FFMPEG_SENDER_ERR_NOT_SUPPORT;
    }     
    
    codec = avcodec_find_encoder(codec_id);        
    if (codec == NULL) {
        STDERR_LOG(LOG_LEVEL_ERR, "Could not find decoder for '%s'\n",
                avcodec_get_name(codec_id));
        return FFMPEG_SENDER_ERR_NOT_SUPPORT;
    } 
    
    stream = avformat_new_stream(fmt_ctx, codec);
    if (!stream) {
        STDERR_LOG(LOG_LEVEL_ERR, "Could not allocate stream\n");
        return FFMPEG_SENDER_ERR_GENERAL;
    }
    stream->id = stream->index;
    out_codec_context_ = stream->codec;            
    
    out_codec_context_->channels       = in_codec_context_->channels;
    out_codec_context_->channel_layout = av_get_default_channel_layout(in_codec_context_->channels);
    out_codec_context_->sample_rate    = in_codec_context_->sample_rate;
    out_codec_context_->sample_fmt     = (codec->sample_fmts)?
                                         codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;

    /** Allow the use of the experimental AAC encoder */
    out_codec_context_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    /** Set the sample rate for the container. */
    stream->time_base.den = in_codec_context_->sample_rate;
    stream->time_base.num = 1;
    

    /**
     * Some container formats (like MP4) require global headers to be present
     * Mark the encoder so that it behaves accordingly.
     */
    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        out_codec_context_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  
    
    
    /** Open the encoder for the audio stream to use it later. */
    ret = avcodec_open2(out_codec_context_, codec, NULL);
    if(ret < 0){
        out_codec_context_ = NULL;
        STDERR_LOG(LOG_LEVEL_ERR, "Could not open output code (%s) context: %s\n",
                avcodec_get_name(codec_id), av_err2str(ret));        
        return FFMPEG_SENDER_ERR_GENERAL;                
    } 
    
    stream_ = stream;

/*    
        printf("aac time_base num/den: %d/%d\n", 
           stream_->time_base.num, 
           stream_->time_base.den);    
*/    
    return 0;
                                        
}
int AacMuxParser::InitResampler()
{
    using namespace stream_switch; 
    int ret;

    /**
     * Create a resampler context for the conversion.
     * Set the conversion parameters.
     */
    resample_context_ = 
        swr_alloc_set_opts(NULL,
                           out_codec_context_->channel_layout,
                           out_codec_context_->sample_fmt,
                           out_codec_context_->sample_rate,
                           in_codec_context_->channel_layout,
                           in_codec_context_->sample_fmt,
                           in_codec_context_->sample_rate,
                           0, NULL);
    if (!resample_context_) {
        STDERR_LOG(LOG_LEVEL_ERR, "Could not allocate resample context\n");
        return FFMPEG_SENDER_ERR_GENERAL;               
    }

    /** Open the resampler with the specified parameters. */
    ret = swr_init(resample_context_);
    if (ret < 0) {
        swr_free(&resample_context_);
        STDERR_LOG(LOG_LEVEL_ERR, "Could not open resample context: %s\n",
                av_err2str(ret));
        return FFMPEG_SENDER_ERR_GENERAL;    
    }   
    return 0;
        
}

int AacMuxParser::InitAudioFifo()
{
    using namespace stream_switch; 
    /** Create the FIFO buffer based on the specified output sample format. */
    if (!(fifo_ = av_audio_fifo_alloc(out_codec_context_->sample_fmt,
                                      out_codec_context_->channels, 2048))) {
        STDERR_LOG(LOG_LEVEL_ERR, "Could not allocate audio FIFO\n");
        return FFMPEG_SENDER_ERR_GENERAL;            
    }
    return 0;    
}

int AacMuxParser::InitInputPacket(const stream_switch::MediaFrameInfo *frame_info, 
                                  const char * frame_data,
                                  size_t frame_size,
                                  struct timeval *base_timestamp,
                                  AVPacket *input_pkt)
{

    int ret = 0;
    
    if(!is_init_){
        return FFMPEG_SENDER_ERR_GENERAL;
    }    
    
    if(in_codec_context_ == NULL){        
        return FFMPEG_SENDER_ERR_GENERAL;
    }
    if(frame_info == NULL){
        return FFMPEG_SENDER_ERR_GENERAL;
    } 

   
    //write the frame to input_pkt, no cached mechanism
    if(frame_info->frame_type == stream_switch::MEDIA_FRAME_TYPE_KEY_FRAME){
        input_pkt->flags |= AV_PKT_FLAG_KEY;
    }
    
    //calculate pts&dts
    double ts_delta = 0.0;
    if(base_timestamp->tv_sec == 0 && base_timestamp->tv_usec == 0){
        (*base_timestamp) =  frame_info->timestamp;
        ts_delta = 0.0;
    }else{
        ts_delta = 
            (double) (frame_info->timestamp.tv_sec - base_timestamp->tv_sec) + 
            (((double)(frame_info->timestamp.tv_usec - base_timestamp->tv_usec)) / 1000000.0);
        if(ts_delta < 0.0){
            //ts_delta = 0.0;
            return 0; //drop frames before base
        }            
    }
        
    input_pkt->pts = input_pkt->dts = 
        (int64_t)(ts_delta * in_codec_context_->time_base.den / in_codec_context_->time_base.num);
    input_pkt->data = (uint8_t *)frame_data;
    input_pkt->size = frame_size;    
    
    return 1;
                                      
}


int AacMuxParser::FlushInternal()
{
    using namespace stream_switch; 
    int ret = 0;
    while (av_audio_fifo_size(fifo_) > 0){
        /** Temporary storage of the output samples of the frame written to the file. */
        AVFrame *output_frame = av_frame_alloc();
        /**
         * Use the maximum number of possible samples per frame.
         * If there is less than the maximum possible frame size in the FIFO
         * buffer use this number. Otherwise, use the maximum possible frame size
         */
        const int frame_size = FFMIN(av_audio_fifo_size(fifo_),
                                     out_codec_context_->frame_size);

        /** Initialize temporary storage for one output frame. */
        ret = InitOutputFrame(output_frame, out_codec_context_, frame_size);
        if (ret){
            av_frame_free(&output_frame); 
            return ret;
        }

        /**
         * Read as many samples from the FIFO buffer as required to fill the frame.
         * The samples are stored in the frame temporarily.
         */
        if (ReadAudioFifo((void **)output_frame->data, frame_size, &(output_frame->pts)) < frame_size) {
            STDERR_LOG(LOG_LEVEL_ERR, "Could not read data from FIFO\n"); 
            av_frame_free(&output_frame);
            return -1;
        }

        /** Encode one frame worth of audio samples. */
        if (ret = EncodeAudioFrame(output_frame, NULL)) {
            av_frame_free(&output_frame);
            return ret;
        }
        
        av_frame_free(&output_frame);  
    }

    /* flush the encoder */    
    int data_present = 0;
    do{
        EncodeAudioFrame(NULL, &data_present);    
    }while(data_present != 0);

    return 0;    
}

int AacMuxParser::ReadAudioFifo(void ** data, int nb_samples, int64_t *pts)
{
    int samples_read;
    samples_read = av_audio_fifo_read(fifo_, (void **)data, nb_samples);
    *pts = fifo_pts_;
    fifo_pts_ += samples_read;
    return samples_read;
}

int AacMuxParser::WriteAudioFifo(void ** data, int nb_samples, int64_t pts)
{
    int samples_write;
    int org_fifo_size;
    
    org_fifo_size = av_audio_fifo_size(fifo_);
    samples_write = av_audio_fifo_write(fifo_, data, nb_samples);
    // update the fifo pts
    if (org_fifo_size == 0){
        fifo_pts_ = pts;
    }
    return samples_write;
}

void AacMuxParser::CleanupAudioFifo()
{
    av_audio_fifo_reset(fifo_);
    fifo_pts_ = 0;
}

bool AacMuxParser::CheckAudioFifoPts(int64_t pts, int64_t sample_rate)
{
    if(av_audio_fifo_size(fifo_) == 0){
        return false;
    }else{
        int64_t last_pts = fifo_pts_ + av_audio_fifo_size(fifo_);
        int64_t delta_pts = (pts>=last_pts)?(pts - last_pts):(last_pts - pts);
        
        return (delta_pts * 1000 / sample_rate) > 300; //offset over 300ms
    }
    
}

/** Encode one frame worth of audio to the output file. */
int AacMuxParser::EncodeAudioFrame(AVFrame *frame,
                            int *data_present)
{
    using namespace stream_switch; 
    /** Packet used for temporary storage. */
    AVPacket output_packet = { 0 };
    int error;
    int tmp_data_present = 0;
    
    if (data_present == NULL){
        data_present = &tmp_data_present;
    }
    //printf("get here %d\n", __LINE__);

    av_init_packet(&output_packet);
    /** Set the packet data and size so that it is recognized as being empty. */
    output_packet.data = NULL;
    output_packet.size = 0;
    
    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    if ((error = avcodec_encode_audio2(out_codec_context_, &output_packet,
                                       frame, data_present)) < 0) {
        av_free_packet(&output_packet);
        STDERR_LOG(LOG_LEVEL_ERR, 
            "Could not encode frame (error '%s')\n", 
            av_err2str(error));             
        return FFMPEG_SENDER_ERR_CODEC; 
    }

    /** Write one audio frame from the temporary packet to the output file. */
    if (*data_present) {
        //printf("get here %d\n", __LINE__);
        printf("packet pts/duration/size: %lld/%ld\n", 
               output_packet.pts, output_packet.duration, output_packet.size);
        output_packet.stream_index = stream_->index;
        av_packet_rescale_ts(&output_packet,
                             out_codec_context_->time_base,
                             stream_->time_base);           
        muxer_->StartIO();
        error = av_interleaved_write_frame(fmt_ctx_, &output_packet);
        muxer_->StopIO();
        if (error < 0) {
            STDERR_LOG(LOG_LEVEL_ERR, 
                "Could not write frame (error '%s')\n", 
                av_err2str(error));             
            return FFMPEG_SENDER_ERR_IO;             
        }
    }

    return 0;
}
