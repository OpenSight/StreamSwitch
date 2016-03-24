/**
 * This file is part of ffmpeg_sender, which belongs to StreamSwitch
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
 * stsw_stream_mux_parser.cc
 *      StreamMuxParser class implementation file, define methods of the StreamMuxParser 
 * class. 
 *      StreamMuxParser is the default parser for a ffmpeg AV stream, other 
 * parser can inherit this class to override its methods for a specified 
 * codec. All other streams would be associated this default parser
 * 
 * author: OpenSight Team
 * date: 2015-11-27
**/ 

#include "stsw_stream_mux_parser.h"

#include <stdint.h>


#include "../stsw_ffmpeg_muxer.h"
#include "../stsw_ffmpeg_sender_global.h"
#include "../stsw_log.h"

#include "stsw_h264or5_mux_parser.h"
#include "stsw_mpeg4_video_mux_parser.h"
#include "stsw_aac_mux_parser.h"


extern "C"{

 
#include <libavcodec/avcodec.h>      
}

enum AVCodecID CodecIdFromName(std::string codec_name);


StreamMuxParser::StreamMuxParser()
:is_init_(false), muxer_(NULL),  fmt_ctx_(NULL), stream_(NULL)
{

}
StreamMuxParser::~StreamMuxParser()
{
    
}
int StreamMuxParser::Init(FFmpegMuxer * muxer, 
                          const std::string &codec_name, 
                          const stream_switch::SubStreamMetadata &sub_metadata, 
                          AVFormatContext *fmt_ctx)
{   
    using namespace stream_switch; 
    int ret = 0;
    if(is_init_){
        return 0;
    }
    
    if(codec_name != sub_metadata.codec_name){
        STDERR_LOG(LOG_LEVEL_ERR, 
            "This parser does not support transcoding from codec %s to %s\n", 
            sub_metadata.codec_name.c_str(), codec_name.c_str());             
        return FFMPEG_SENDER_ERR_NOT_SUPPORT;        
    }
    
    //setup stream
    if((sub_metadata.media_type == SUB_STREAM_MEIDA_TYPE_VIDEO || 
        sub_metadata.media_type == SUB_STREAM_MEIDA_TYPE_AUDIO) && 
       sub_metadata.direction == SUB_STREAM_DIRECTION_OUTBOUND){
        //only support outbound video/audio stream now
        AVCodecContext *c = NULL;
        AVCodec *codec = NULL;
        AVStream * stream = NULL;
        int i;
        enum AVCodecID codec_id = AV_CODEC_ID_NONE;
        
        codec_id = CodecIdFromName(codec_name);
        if(codec_id == AV_CODEC_ID_NONE){
            STDERR_LOG(LOG_LEVEL_ERR, "codec %s not support\n", 
                   codec_name.c_str());             
            return FFMPEG_SENDER_ERR_NOT_SUPPORT;
        }
        /* find the encoder, 
         * use decoder instead of encoder, 
         * because can make use of extra_data to set up the codec context
         */
        /*
        if(sub_metadata.extra_data.size() == 0){
            codec = avcodec_find_encoder(codec_id);
        }*/
        if(codec == NULL){
            codec = avcodec_find_decoder(codec_id);
        }
        
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
        c = stream->codec;        

        switch (codec->type) {
        case AVMEDIA_TYPE_AUDIO:
            c->sample_fmt  = codec->sample_fmts ?
                codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            //c->bit_rate    = 0;
            if(sub_metadata.media_param.audio.samples_per_second != 0){
                c->sample_rate = 
                    sub_metadata.media_param.audio.samples_per_second;
            }else{
                c->sample_rate = 8000; //the lowest rate
                if (codec->supported_samplerates) {
                    c->sample_rate = codec->supported_samplerates[0];
                    for (i = 0; codec->supported_samplerates[i]; i++) {
                        if (codec->supported_samplerates[i] == 8000)
                            c->sample_rate = 8000;
                    }
                }
            }        

            if(sub_metadata.media_param.audio.channels != 0){
                c->channels = sub_metadata.media_param.audio.channels;
                c->channel_layout = av_get_default_channel_layout(c->channels);
            }else{
                c->channel_layout = AV_CH_LAYOUT_MONO;
                if (codec->channel_layouts) {
                    c->channel_layout = codec->channel_layouts[0];
                    for (i = 0; codec->channel_layouts[i]; i++) {
                        if (codec->channel_layouts[i] == AV_CH_LAYOUT_MONO)
                            c->channel_layout = AV_CH_LAYOUT_MONO;
                    }
                }
                c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
                
            }
            stream->time_base = (AVRational){ 1, c->sample_rate };
            break;  
        case AVMEDIA_TYPE_VIDEO:
            /* Resolution must be a multiple of two. */
            if(sub_metadata.media_param.video.height != 0 &&
               sub_metadata.media_param.video.width != 0){
                c->width = sub_metadata.media_param.video.width;
                c->height = sub_metadata.media_param.video.height;
            }else{
                /*default is 1080P*/
                c->width    = 1920;
                c->height   = 1080;                
            }
            /* timebase: default is 1/90K */
            stream->time_base = (AVRational){ 1, 90000 };
            c->time_base       = stream->time_base;
            if(sub_metadata.media_param.video.fps != 0){
                stream->avg_frame_rate.num = sub_metadata.media_param.video.fps;
                stream->avg_frame_rate.den = 1;
            }
            if(sub_metadata.media_param.video.gov != 0){
                c->gop_size = sub_metadata.media_param.video.gov;
            }
            c->pix_fmt       = AV_PIX_FMT_YUV420P;
            c->max_b_frames = 0;
            break;
        default:
            break;   
        }

        if(sub_metadata.extra_data.size() != 0){    
            c->extradata_size = sub_metadata.extra_data.size();
            c->extradata = (uint8_t *)av_mallocz(c->extradata_size);
            memcpy(c->extradata, sub_metadata.extra_data.data(), c->extradata_size);                               
        }      
 
        ret = DoExtraDataInit(muxer, sub_metadata, fmt_ctx, stream);
        if(ret){
            return ret;
        }        
      
        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;                
        
        stream_ = stream;
       
    }else{
        //for stream of other type, ignore
        stream_ = NULL;
    }    
    
    muxer_ = muxer;
    fmt_ctx_ = fmt_ctx;
    gop_started_ = false;
    codec_name_ = codec_name;

    is_init_ = true;
    return 0;
}
void StreamMuxParser::Uninit()
{
    if(!is_init_){
        return;
    }
    is_init_ = false;

    muxer_ = NULL;
    stream_ = NULL;
    fmt_ctx_ = NULL;
    
   
    
    return;
}

int StreamMuxParser::DoExtraDataInit(FFmpegMuxer * muxer, 
                             const stream_switch::SubStreamMetadata &sub_metadata, 
                             AVFormatContext *fmt_ctx, 
                             AVStream * stream)
{
    using namespace stream_switch; 
    AVCodecContext *c = stream->codec;
    int ret = 0;
    if(sub_metadata.extra_data.size() != 0){    
        ret = avcodec_open2(c, NULL, NULL);
        if(ret){
            STDERR_LOG(LOG_LEVEL_ERR, "Could not open code (%s) context: %s\n",
                    avcodec_get_name(c->codec_id), av_err2str(ret));
            return FFMPEG_SENDER_ERR_GENERAL;                
        }
        avcodec_close(c);                                
    }
    return 0;    
}

int StreamMuxParser::Parse(const stream_switch::MediaFrameInfo *frame_info, 
                           const char * frame_data,
                           size_t frame_size,
                           struct timeval *base_timestamp,
                           AVPacket *opkt)
{
    int ret = 0;
    
    if(!is_init_){
        return FFMPEG_SENDER_ERR_GENERAL;
    }    
    
    if(frame_info == NULL){
        return 0; //no further packet to write to the context
    }
    
    if(stream_ == NULL){
        //no associate any stream, just drop the frame
        return 0;
    }
    
    //write the frame to opkt, no cached mechanism
    if(frame_info->frame_type == stream_switch::MEDIA_FRAME_TYPE_KEY_FRAME){
        opkt->flags |= AV_PKT_FLAG_KEY;
        gop_started_ = true;
    }
    if(!gop_started_){
        //drop all frames before key frame
        return 0;
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
            
            return 0; //drop all frames before base
        }            
    }
    
    
    opkt->pts = opkt->dts = 
        (int64_t)(ts_delta * stream_->time_base.den / stream_->time_base.num);
    opkt->stream_index = stream_->index;

    opkt->data = (uint8_t *)frame_data;
    opkt->size = frame_size;
    
    
    return 1;
}

void StreamMuxParser::Flush()
{
    //nothing to do for default mux stream parser
}




template<class T>
StreamMuxParser* StreamMuxParserFatcory()
{
	return new T;
}

struct MuxParserInfo {
    enum AVCodecID codec_id;
    StreamMuxParser* (*factory)();
    const char codec_name[32];
};

//FIXME this should be simplified!
static const MuxParserInfo parser_infos[] = {
   { AV_CODEC_ID_H264, StreamMuxParserFatcory<H264or5MuxParser>, "H264" },
   { AV_CODEC_ID_H265, StreamMuxParserFatcory<H264or5MuxParser>, "H265" }, 
   { AV_CODEC_ID_MPEG4, StreamMuxParserFatcory<Mpeg4VideoMuxParser>, "MP4V-ES" },   
   { AV_CODEC_ID_AAC, StreamMuxParserFatcory<AacMuxParser>, "AAC" },   
   { AV_CODEC_ID_AMR_NB, NULL, "AMR" },
   { AV_CODEC_ID_PCM_MULAW, NULL, "PCMU"},
   { AV_CODEC_ID_PCM_ALAW, NULL, "PCMA"},
   { AV_CODEC_ID_NONE, NULL, "NONE"}//XXX ...
};


enum AVCodecID CodecIdFromName(std::string codec_name)
{
    const MuxParserInfo *parser_info = parser_infos;
    while (parser_info->codec_id != AV_CODEC_ID_NONE) {
        if (strcasecmp(codec_name.c_str(), parser_info->codec_name) == 0){
            return parser_info->codec_id;
        }            
        parser_info++;
    }
    AVCodec *c = NULL;
    while ((c = av_codec_next(c))) {
        if(strcasecmp(codec_name.c_str(),c->name) == 0){
            return c->id;
        }        
    }    
    return AV_CODEC_ID_NONE;
}


StreamMuxParser * NewStreamMuxParser(std::string codec_name)
{
    const MuxParserInfo *parser_info = parser_infos;
    while (parser_info->codec_id != AV_CODEC_ID_NONE) {
        if (strcasecmp(codec_name.c_str(), parser_info->codec_name) == 0){
            if(parser_info->factory != NULL){
                return parser_info->factory();
            }else{
                return new StreamMuxParser();
            }
        }            
        parser_info++;
    }
    return new StreamMuxParser();
}
