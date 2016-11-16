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
 * stsw_h264or5_parser.h
 *      H264or5Parser class header file, define intefaces of the H264or5Parser 
 * class. 
 *      H264or5Parser is the child class of StreamParser, whichs overrides some 
 * of the parent's methods for h264 or h265, like update metadata for h264/h265
 * 
 * author: OpenSight Team
 * date: 2015-10-15
**/ 

#ifndef STSW_H264OR5_PARSER_H
#define STSW_H264OR5_PARSER_H


#include "stsw_stream_parser.h"

class H264or5Parser: public StreamParser{
  
public:
    H264or5Parser();
    virtual ~H264or5Parser();
    
    virtual int Init(FFmpegDemuxer *demuxer, int stream_index);

    virtual int Parse(stream_switch::MediaFrameInfo *frame_info, 
                      AVPacket *pkt, 
                      bool* is_meta_changed);


    virtual bool IsMetaReady();

protected:
    virtual int DoUpdateMeta(AVPacket *pkt, bool* is_meta_changed);
    virtual int GetExtraDataSize(AVPacket *pkt);

    
    virtual bool IsVPS(uint8_t nal_unit_type);
    virtual bool IsSPS(uint8_t nal_unit_type);
    virtual bool IsPPS(uint8_t nal_unit_type);
    virtual bool IsVCL(uint8_t nal_unit_type);
    
    virtual int AvcCToAnnexB(std::string &extra_data);

    int h_number_; 
    
    std::string key_frame_buf_;
    size_t extradata_size_;
    

};

#endif