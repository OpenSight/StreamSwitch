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
        return StreamMuxParser::Init(muxer, codec_name, sub_metadata, fmt_ctx_);        
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

    is_init_ = true; 
    
#define DUMP_INPUT_CONTEXT
#ifdef DUMP_INPUT_CONTEXT
    char buf[256];
    avcodec_string(buf, sizeof(buf), in_codec_context_, 0);    
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO,  
            "AacMuxParser::Init(): Transcoding from the codec contexst (%s) to %s\n",
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
    using namespace stream_switch; 
    int ret = 0;
    if(!transcoding_){
        StreamMuxParser::Flush();
        return;
    }
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
            return;
        }

        /**
         * Read as many samples from the FIFO buffer as required to fill the frame.
         * The samples are stored in the frame temporarily.
         */
        if (ReadAudioFifo((void **)output_frame->data, frame_size, &(output_frame->pts)) < frame_size) {
            STDERR_LOG(LOG_LEVEL_ERR, "Could not read data from FIFO\n"); 
            av_frame_free(&output_frame);
            return;
        }

        /** Encode one frame worth of audio samples. */
        if (EncodeAudioFrame(output_frame, NULL)) {
            av_frame_free(&output_frame);
            return;
        }
        
        av_frame_free(&output_frame);  
    }
    
    /* flush the encoder */
    EncodeAudioFrame(NULL, NULL);

    return;
}
    
int AacMuxParser::Parse(const stream_switch::MediaFrameInfo *frame_info, 
                      const char * frame_data,
                      size_t frame_size,
                      struct timeval *base_timestamp,
                      AVPacket *opkt)
{
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
        uint8_t **converted_input_samples = NULL;
        
        //1. get AVPacket from frame_info
        ret = StreamMuxParser::Parse(frame_info, frame_data, frame_size, 
                                     base_timestamp, &input_pkt);
        if(ret <= 0){
            return ret;
        }
        
        //1. decode packet
        /**
         * Decode the audio frame stored in the temporary packet.
         * The input audio stream decoder is used to do this.
         */
        if ((error = avcodec_decode_audio4(in_codec_context_, input_frame_,
                                           &data_present, &input_pkt)) < 0) {
            STDERR_LOG(LOG_LEVEL_ERR, "Could not decode frame (error '%s')\n",
                       get_error_text(error)); 
            av_free_packet(&input_packet);
            return error;
        }
        
        
        if (data_present) {
            /** Initialize the temporary storage for the converted input samples. */
            if (init_converted_samples(&converted_input_samples, output_codec_context,
                                       input_frame->nb_samples)){
                                           
            }
                

        /**
         * Convert the input samples to the desired output sample format.
         * This requires a temporary storage provided by converted_input_samples.
         */
        if (convert_samples((const uint8_t**)input_frame->extended_data, converted_input_samples,
                            input_frame->nb_samples, resampler_context))
            goto cleanup;

        /** Add the converted input samples to the FIFO buffer for later processing. */
        if (add_samples_to_fifo(fifo, converted_input_samples,
                                input_frame->nb_samples))
            goto cleanup;
        ret = 0;
    
        //2. resample
        
        //3. check pts consistent and flush
        
        //4. write to fifo
        
    }
    
    ret = 0;
    while (av_audio_fifo_size(fifo_) >= output_frame_size) {  
        //5. get frame from fifo
        
        //6. encode frame 
    }
    
    
    
    return ret;
                          
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
    out_codec_context_->sample_fmt     = codec->sample_fmts[0];

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
        STDERR_LOG(LOG_LEVEL_ERR, "Could not open output code (%s) context: %s\n",
                avcodec_get_name(codec_id), av_err2str(ret));
        return FFMPEG_SENDER_ERR_GENERAL;                
    } 
    
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
    }else{
        fifo_pts_ += samples_write;
    }
    return samples_write;
}


/** Encode one frame worth of audio to the output file. */
int AacMuxParser::EncodeAudioFrame(AVFrame *frame,
                            int *data_present)
{
    using namespace stream_switch; 
    /** Packet used for temporary storage. */
    AVPacket output_packet = { 0 };;
    int error;
    int tmp_data_present = 0;
    
    if (data_present == NULL){
        data_present = &tmp_data_present;
    }
    

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
