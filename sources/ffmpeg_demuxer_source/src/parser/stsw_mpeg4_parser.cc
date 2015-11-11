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
 * stsw_mpeg4_parser.cc
 *      H264or5Parser class implementation file, define methods of the H264or5Parser 
 * class. 
 *      H264or5Parser is the child class of StreamParser, whichs overrides some 
 * of the parent's methods for h264 or h265, like update metadata for h264/h265
 * 
 * author: jamken
 * date: 2015-11-3
**/ 

#include "stsw_mpeg4_parser.h"
#include "../stsw_ffmpeg_demuxer.h"
#include "../stsw_ffmpeg_source_global.h"
#include "../stsw_log.h"

extern "C"{

 
#include <libavcodec/avcodec.h>      
}

int Mpeg4Parser::Init(FFmpegDemuxer *demuxer, int stream_index)
{

    //never use the default extra data which is provided by ffmpeg for mpeg4 video,
    //Instead, extract it from the key frame by myself
    demuxer->meta_.sub_streams[stream_index_].extra_data.clear(); 
    
    return StreamParser::Init(demuxer, stream_index);
}


bool Mpeg4Parser::IsMetaReady()
{
    if(!is_init_){
        return false;
    }
    return demuxer_->meta_.sub_streams[stream_index_].extra_data.size() > 0;
}

int Mpeg4Parser::DoUpdateMeta(AVPacket *pkt, bool* is_meta_changed)
{
    bool meta_changed = false;    
    if(pkt->flags & AV_PKT_FLAG_KEY){
        //check basic params
        AVCodecContext *codec= stream_->codec;            
        if(codec->width != 0 && codec->height != 0){
            uint32_t height = codec->height;
            uint32_t width = codec->width;
            if(codec->height !=  
               demuxer_->meta_.sub_streams[stream_index_].media_param.video.height || 
               codec->width != 
               demuxer_->meta_.sub_streams[stream_index_].media_param.video.width){
                
                demuxer_->meta_.sub_streams[stream_index_].media_param.video.height = 
                    codec->height; 
                demuxer_->meta_.sub_streams[stream_index_].media_param.video.width =
                    codec->width;
                meta_changed = true;
            }
        } 
        if(stream_->avg_frame_rate.num != 0 && stream_->avg_frame_rate.den != 0){
            uint32_t fps = stream_->avg_frame_rate.num / stream_->avg_frame_rate.num;
            if(fps != demuxer_->meta_.sub_streams[stream_index_].media_param.fps){
                demuxer_->meta_.sub_streams[stream_index_].media_param.fps = fps;
                meta_changed = true;
            }
        }        
        //check extra data
        int extra_size = GetExtraDataSize(pkt);
        if(extra_size != 0){
            std::string new_extra((const char*)pkt->data, (size_t)extra_size);
            if(new_extra != 
               demuxer_->meta_.sub_streams[stream_index_].extra_data){
                demuxer_->meta_.sub_streams[stream_index_].extra_data = new_extra;
                meta_changed = true;
            }
        }
    
    }
    
    if(is_meta_changed != NULL){
        (*is_meta_changed) = meta_changed;
    }        

    return 0;
}
#define GROUP_VOP_START_CODE      0xB3
#define VOP_START_CODE            0xB6
#define VISUAL_OBJECT_SEQUENCE_START_CODE      0xB0

int Mpeg4Parser::GetExtraDataSize(AVPacket *pkt)
{
    uint8_t *p = pkt->data;
    int extra_size = 0;
    int len = pkt->size;
    if (len >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 1 && 
        p[3] == VISUAL_OBJECT_SEQUENCE_START_CODE) {    
        unsigned i;

        // The start of this frame - up to the first GROUP_VOP_START_CODE
        // or VOP_START_CODE - is stream configuration information.  Save this:
        for (i = 7; i < len; ++i) {
            if ((p[i] == GROUP_VOP_START_CODE /*GROUP_VOP_START_CODE*/ ||
                 p[i] == VOP_START_CODE /*VOP_START_CODE*/)
                && p[i-1] == 1 && p[i-2] == 0 && p[i-3] == 0) {
                break; // The configuration information ends here
            }
        }
        if(i < len){
            extra_size = i;
        }else{
            extra_size = len;
        }
    }//if (len >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 1 && 
    return extra_size;
    
}
