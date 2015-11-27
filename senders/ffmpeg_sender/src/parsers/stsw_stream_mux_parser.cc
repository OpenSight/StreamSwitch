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
 * stsw_stream_parser.cc
 *      StreamParser class implementation file, define methods of the StreamParser 
 * class. 
 *      StreamParser is the default parser for a ffmpeg AV stream, other 
 * parser can inherit this class to override its methods for a specified 
 * codec. All other streams would be associated this default parser
 * 
 * author: jamken
 * date: 2015-10-27
**/ 

#include "stsw_stream_parser.h"

#include <stdint.h>


#include "../stsw_ffmpeg_demuxer.h"
#include "../stsw_ffmpeg_source_global.h"
#include "stsw_h264or5_parser.h"
#include "stsw_mpeg4_parser.h"

extern "C"{

 
#include <libavcodec/avcodec.h>      
}

StreamMuxParser::StreamMuxParser()
:is_init_(false), muxer_(NULL),  fmt_ctx_(NULL), stream_(NULL)
{

}
StreamMuxParser::~StreamMuxParser()
{
    
}
int StreamMuxParser::Init(FFmpegMuxer * muxer, 
                          const stream_switch::SubStreamMetadata &sub_metadata, 
                          AVFormatContext *fmt_ctx)
{   
    int ret = 0;
    if(is_init_){
        return 0;
    }
    //setup stream
    
    
    
    muxer_ = muxer;
    fmt_ctx_ = fmt_ctx

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
int StreamMuxParser::Parse(stream_switch::MediaFrameInfo *frame_info, 
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
            ts_delta = 0.0;
        }            
    }
    opkt->pts = opkt->dts = 
        (int64_t)(ts_delta * stream_->time_base.den / stream_->time_base.num);
    opkt->stream_index = stream_->index;
    if(frame_info->frame_type == MEDIA_FRAME_TYPE_KEY_FRAME){
        opkt->flags |= AV_PKT_FLAG_KEY;
    }
    opkt->data = frame_data;
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
    const int codec_id;
    StreamMuxParser* (*factory)();
    const char codec_name[11];
};

//FIXME this should be simplified!
static const MuxParserInfo parser_infos[] = {
   { AV_CODEC_ID_AMR_NB, NULL, "AMR" },
   { AV_CODEC_ID_PCM_MULAW, NULL, "PCMU"},
   { AV_CODEC_ID_PCM_ALAW, NULL, "PCMA"},
   { AV_CODEC_ID_NONE, NULL, "NONE"}//XXX ...
};


static int CodecIdFromName(std::string codec_name)
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


StreamMuxParser * NewStreamParser(std::string codec_name)
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
