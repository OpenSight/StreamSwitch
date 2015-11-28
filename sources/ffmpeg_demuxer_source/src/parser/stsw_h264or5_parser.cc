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
 * stsw_h264or5_parser.cc
 *      H264or5Parser class implementation file, define methods of the H264or5Parser 
 * class. 
 *      H264or5Parser is the child class of StreamParser, whichs overrides some 
 * of the parent's methods for h264 or h265, like update metadata for h264/h265
 * 
 * author: jamken
 * date: 2015-11-3
**/ 

#include "stsw_h264or5_parser.h"
#include "../stsw_ffmpeg_demuxer.h"
#include "../stsw_ffmpeg_source_global.h"
#include "../stsw_log.h"

#include <arpa/inet.h>

extern "C"{

 
#include <libavcodec/avcodec.h>      
}

H264or5Parser::H264or5Parser()
:h_number_(264)
{

}
H264or5Parser::~H264or5Parser()
{
    
}
int H264or5Parser::Init(FFmpegDemuxer *demuxer, int stream_index)
{
    AVStream *st= demuxer->fmt_ctx_->streams[stream_index];
    AVCodecContext *codec= st->codec;      

    //printf("file:%s, line:%d\n", __FILE__, __LINE__); 

    
    if(codec->codec_id == AV_CODEC_ID_H264){
        h_number_ = 264;
    }else if(codec->codec_id == AV_CODEC_ID_H265){
        h_number_ = 265;
    }else{
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
            "H264or5Parser cannot support codec(%d)\n", 
                (int)codec->codec_id);  
        return -1;
    }
    //Never use the default extra data which is provided by ffmpeg for h264/h265,
    //Instead, extract it from the key frame by myself
    demuxer->meta_.sub_streams[stream_index_].extra_data.clear(); 
    
    return StreamParser::Init(demuxer, stream_index);
}

int H264or5Parser::Parse(stream_switch::MediaFrameInfo *frame_info, 
                         AVPacket *pkt, 
                         bool* is_meta_changed)
{   
    //support avc1 packet format
    static const char start_code[4] = { 0, 0, 0, 1 };
    if(memcmp(start_code, pkt->data, 4) != 0)
    {//is avc1 code, have no start code of H264
        int len = 0;
        uint8_t *p = pkt->data;

        do{//add start_code for each NAL, one frame may have multi NALs.
            len = ntohl(*((long*)p));
            memcpy(p, start_code, 4);

            p += 4;
            p += len;
            if(p >= pkt->data + pkt->size)
            {
                break;
            }
        } while (1);
    }    
    
    return StreamParser::Parse(frame_info, pkt, is_meta_changed);
                             
}

bool H264or5Parser::IsMetaReady()
{
    if(!is_init_){
        return false;
    }
    return demuxer_->meta_.sub_streams[stream_index_].extra_data.size() > 0;
}

int H264or5Parser::DoUpdateMeta(AVPacket *pkt, bool* is_meta_changed)
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
            uint32_t fps = stream_->avg_frame_rate.num / stream_->avg_frame_rate.den;
            if(fps != demuxer_->meta_.sub_streams[stream_index_].media_param.video.fps){
                demuxer_->meta_.sub_streams[stream_index_].media_param.video.fps = fps;
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

int H264or5Parser::GetExtraDataSize(AVPacket *pkt)
{
    bool vps = false, sps = false, pps = false;
    uint8_t *p, *end , *start;   
    int extra_size = 0;
    start = pkt->data;
    end = pkt->data + pkt->size;

    for (p = start; p < end - 4; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            uint8_t nal_unit_type;
            if (h_number_ == 264 && end - p >= 5) {
                nal_unit_type = p[4]&0x1F;
            } else if (h_number_ == 265 && (end - p) >= 6) {
                nal_unit_type = (p[4]&0x7E)>>1;
            } else {
                nal_unit_type = 0xff;
            }
            
            if(IsVPS(nal_unit_type)){
                vps = true;
            }else if(IsSPS(nal_unit_type)){
                sps = true;
            }else if(IsPPS(nal_unit_type)){
                pps = true;
            }else if (IsVCL(nal_unit_type)){
                
                extra_size = p - start;
                break;
            } 
           
        } // if (p[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1) {           
    } // for (p = start; p < end - 4; p++) {   
    if(p == end - 4){
        extra_size = pkt->size; //all is extra data, 
                                //should not arrive here
    }
        
    if(extra_size != 0){
        if (h_number_ == 264 && sps && pps){
            return extra_size;
        }else if (h_number_ == 265 && vps && sps && pps){
            return extra_size;
        }
    }

    return 0;
}

bool H264or5Parser::IsVPS(uint8_t nal_unit_type) {
  // VPS NAL units occur in H.265 only:
  return h_number_ == 265 && nal_unit_type == 32;
}

bool H264or5Parser::IsSPS(uint8_t nal_unit_type) {
  return h_number_ == 264 ? nal_unit_type == 7 : nal_unit_type == 33;
}

bool H264or5Parser::IsPPS(uint8_t nal_unit_type) {
  return h_number_ == 264 ? nal_unit_type == 8 : nal_unit_type == 34;
}

bool H264or5Parser::IsVCL(uint8_t nal_unit_type) {
  return h_number_ == 264
    ? (nal_unit_type <= 5 && nal_unit_type > 0)
    : (nal_unit_type <= 31);
}
