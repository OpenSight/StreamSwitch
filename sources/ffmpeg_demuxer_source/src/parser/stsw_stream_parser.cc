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


extern "C"{

 
#include <libavcodec/avcodec.h>      
}

StreamParser::StreamParser()
:is_init_(false), stream_index_(0),  demuxer_(NULL), stream_(NULL), is_live_(true), 
gop_started_(false),
last_pts_(AV_NOPTS_VALUE), last_dur_(0)
{
    last_live_ts_.tv_sec = 0;
    last_live_ts_.tv_usec = 0;
}
StreamParser::~StreamParser()
{
    
}
int StreamParser::Init(FFmpegDemuxer *demuxer, int stream_index)
{   
    int ret = 0;
    if(is_init_){
        return 0;
    }
    demuxer_ = demuxer;
    stream_index_ = stream_index;
    stream_ = demuxer->fmt_ctx_->streams[stream_index];
    is_live_ = 
        (demuxer->meta_.play_type == stream_switch::STREAM_PLAY_TYPE_LIVE);
    gop_started_ = false;
    last_pts_ = AV_NOPTS_VALUE;
    last_dur_ = 0;
    
    last_live_ts_.tv_sec = 0;
    last_live_ts_.tv_usec = 0;  
    
    is_init_ = true;
    return 0;
}
void StreamParser::Uninit()
{
    if(!is_init_){
        return;
    }
    is_init_ = false;
    stream_index_ = 0;
    demuxer_ = NULL;
    stream_ = NULL;
    is_live_ = true;
    gop_started_ = false;
    last_pts_ = AV_NOPTS_VALUE;
    last_dur_ = 0;
    
    last_live_ts_.tv_sec = 0;
    last_live_ts_.tv_usec = 0;     
    
    return;
}
int StreamParser::Parse(stream_switch::MediaFrameInfo *frame_info, 
                        AVPacket *pkt, 
                        bool* is_meta_changed)
{
    int ret = 0;

    if(pkt->flags & AV_PKT_FLAG_CORRUPT){
        //for corrupt packet, juet ignore and drop
        return FFMPEG_SOURCE_ERR_DROP;
    }
    
    ret = DoUpdateFrameInfo(frame_info, pkt);
    if(ret){
        return ret;
    }    
    
    ret = DoUpdateMeta(pkt, is_meta_changed);
    if(ret){
        return ret;
    }
    
    if(frame_info->frame_type == stream_switch::MEDIA_FRAME_TYPE_KEY_FRAME){
        gop_started_ = true;
    }
    
    if(!gop_started_){
        return FFMPEG_SOURCE_ERR_DROP;
    }
    
    return 0;
}
void StreamParser::reset()
{
    gop_started_ = false;
    return;
}

bool StreamParser::IsMetaReady()
{
    return true;
}


int StreamParser::DoUpdateFrameInfo(stream_switch::MediaFrameInfo *frame_info, 
                                    AVPacket *pkt)
{
    frame_info->ssrc = 0;
    frame_info->sub_stream_index = stream_index_;    
    if(pkt->flags & AV_PKT_FLAG_KEY){
        frame_info->frame_type = stream_switch::MEDIA_FRAME_TYPE_KEY_FRAME;
    }else{
        frame_info->frame_type = stream_switch::MEDIA_FRAME_TYPE_DATA_FRAME;
    }    
    //calculate timestamp
    if(is_live_){        
        if(last_pts_ == AV_NOPTS_VALUE){
            //this is the first (has pts) packet to parse, use it as base
            last_pts_ = pkt->pts;
            last_dur_ = pkt->duration;
            gettimeofday(&last_live_ts_, NULL);
            frame_info->timestamp = last_live_ts_;
            
        }else{
            if(pkt->pts == AV_NOPTS_VALUE){
                // no pts for this packet, may be because of B frame situation, 
                // which is not support for live stream, use the last packt time
                frame_info->timestamp = last_live_ts_;
                last_dur_ += pkt->duration;
          
            }else {
                int64_t pts_delta = 0;
                if(pkt->pts < last_pts_ || 
                   (pkt->pts - last_pts_) * stream_->time_base.num / stream_->time_base.den > 60) {
                    //this pts lose sync with the last pts, try to guess their delta 
                    if(last_dur_ > 0){
                        //if duration is known, used last duration as delta
                        pts_delta = last_dur_;
                    }else if (stream_->codec->codec_type == AVMEDIA_TYPE_VIDEO && 
                              stream_->avg_frame_rate.den && 
                              stream_->avg_frame_rate.num){
                        //make use of average fps to calculate delta
                        pts_delta = (stream_->avg_frame_rate.den * stream_->time_base.den) /
                                    (stream_->avg_frame_rate.num * stream_->time_base.num);
                    }else{
                        //no any way to know delta
                        pts_delta = 0;
                    }
                }else{
                    //normal case
                    pts_delta = pkt->pts - last_pts_;
                    
                }
                
                frame_info->timestamp.tv_sec = 
                    (pts_delta * stream_->time_base.num) / stream_->time_base.den + 
                    last_live_ts_.tv_sec;
                frame_info->timestamp.tv_usec = 
                    ((pts_delta * stream_->time_base.num) % stream_->time_base.den) 
                    * 1000000 / stream_->time_base.den + 
                    last_live_ts_.tv_usec;  
                while(frame_info->timestamp.tv_usec >= 1000000){
                    frame_info->timestamp.tv_sec ++;
                    frame_info->timestamp.tv_usec -= 1000000;
                }

                last_pts_ = pkt->pts;
                last_dur_ = pkt->duration;
                last_live_ts_ = frame_info->timestamp;
                
            }//if(pkt->pts == AV_NOPTS_VALUE){             
            
        }//if(pkt->pts == AV_NOPTS_VALUE){
       
    }else{ // replay
        
        int64_t pts;
        //for non-live mode, use pts directly
        if(pkt->pts == AV_NOPTS_VALUE){
            if(last_pts_ != AV_NOPTS_VALUE){
                pts = last_pts_;
            }else{
                pts = 0;
            }            
        }else{
            pts = pkt->pts;
            last_pts_ = pts;
            last_dur_ = pkt->duration;
        }
        
        frame_info->timestamp.tv_sec = 
            (pts * stream_->time_base.num) / stream_->time_base.den;
        frame_info->timestamp.tv_usec = 
            ((pts * stream_->time_base.num) % stream_->time_base.den) 
            * 1000000 / stream_->time_base.den;        
    }   
    return 0;
}


int StreamParser::DoUpdateMeta(AVPacket *pkt, bool* is_meta_changed)
{
    if(is_meta_changed != NULL){
        (*is_meta_changed) = false;
    }
    return 0;
}






template<class T>
StreamParser* StreamParserFatcory()
{
	return new T;
}

struct ParserInfo {
    const int codec_id;
    StreamParser* (*factory)();
    const char name[11];
};

//FIXME this should be simplified!
static const ParserInfo parser_infos[] = {
   { AV_CODEC_ID_H264, NULL, "H264" },
   { AV_CODEC_ID_H265, NULL, "H265" }, 
   { AV_CODEC_ID_MPEG4, NULL, "MP4V-ES" },
   { AV_CODEC_ID_AMR_NB, NULL, "AMR" },
   { AV_CODEC_ID_PCM_MULAW, NULL, "PCMU"},
   { AV_CODEC_ID_PCM_ALAW, NULL, "PCMA"},
   { AV_CODEC_ID_NONE, NULL, "NONE"}//XXX ...
};

const char * CodecNameFromId(int codec_id)
{
    const ParserInfo *parser_info = parser_infos;
    while (parser_info->codec_id != AV_CODEC_ID_NONE) {
        if (parser_info->codec_id == codec_id){
            if(parser_info->name != NULL){
                return parser_info->name;
            }else{
                return avcodec_get_name((enum AVCodecID)codec_id);
            }
        }            
        parser_info++;
    }
    return avcodec_get_name((enum AVCodecID)codec_id);
}
StreamParser * NewStreamParser(int codec_id)
{
    const ParserInfo *parser_info = parser_infos;
    while (parser_info->codec_id != AV_CODEC_ID_NONE) {
        if (parser_info->codec_id == codec_id){
            if(parser_info->factory != NULL){
                return parser_info->factory();
            }else{
                return new StreamParser();
            }
        }            
        parser_info++;
    }
    return new StreamParser();
}
