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
 * stsw_stream_parser.h
 *      StreamParser class header file, define intefaces of the StreamParser 
 * class. 
 *      StreamParser is the default parser for a ffmpeg AV stream, other 
 * parser can inherit this class to override its methods for a specified 
 * codec. All other streams would be associated this default parser
 * 
 * author: jamken
 * date: 2015-10-15
**/ 

#ifndef STSW_STREAM_PARSER_H
#define STSW_STREAM_PARSER_H


#include <time.h>
#include <stream_switch.h>

typedef struct AVPacket AVPacket;
typedef void (*OnErrorFun)(int error_code, void *user_data);

///////////////////////////////////////////////////////////////
//Type
class FFmpegDemuxer;

typedef struct AVStream AVStream;

class StreamParser{
  
public:
    StreamParser();
    virtual ~StreamParser();
    virtual int Init(FFmpegDemuxer *demuxer, int stream_index);
    virtual void Uninit();
    virtual int Parse(stream_switch::MediaFrameInfo *frame_info, 
                      AVPacket *pkt, 
                      bool* is_meta_changed);
    virtual void reset();
    virtual bool IsMetaReady();

protected:
    virtual int DoUpdateFrameInfo(stream_switch::MediaFrameInfo *frame_info, 
                                  AVPacket *pkt);
    virtual int DoUpdateMeta(AVPacket *pkt, bool* is_meta_changed);
    
    bool is_init_;
    int stream_index_;
    FFmpegDemuxer *demuxer_;
    AVStream *stream_;
    bool is_live_;
    bool gop_started_;
    int64_t last_pts_;
    int64_t last_dur_;
    struct timeval last_live_ts_;

};
    

const char * CodecNameFromId(int codec_id);
StreamParser * NewStreamParser(int codec_id);

#endif