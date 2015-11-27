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
 * stsw_stream_mux_parser.h
 *      StreamMuxParser class header file, define intefaces of the StreamMuxParser 
 * class. 
 *      StreamMuxParser is the default parser muxing a StreamSwitch stream to a 
 * ffmpeg context as a ffmpeg AV stream, other mux
 * parser can inherit this class to override its methods for a specified 
 * codec. All other streams would be associated this default mux parser
 * 
 * author: jamken
 * date: 2015-10-15
**/ 

#ifndef STSW_STREAM_MUX_PARSER_H
#define STSW_STREAM_MUX_PARSER_H


#include <time.h>
#include <stream_switch.h>
extern "C"{

#include <libavutil/avutil.h>    
#include <libavformat/avformat.h>      
}


///////////////////////////////////////////////////////////////
//Type
class FFmpegMuxer;

typedef struct AVStream AVStream;

class StreamMuxParser{
  
public:
    StreamParser();
    virtual ~StreamParser();
    virtual int Init(FFmpegMuxer * muxer, 
                     const stream_switch::SubStreamMetadata &sub_metadata, 
                     AVFormatContext *fmt_ctx);
    virtual void Uninit();
    virtual void Flush();    
    
    virtual int Parse(stream_switch::MediaFrameInfo *frame_info, 
                      const char * frame_data,
                      size_t frame_size,
                      struct timeval *base_timestamp,
                      AVPacket *opkt);


protected:


    
    bool is_init_;
    FFmpegMmuxer *muxer_;
    AVFormatContext *fmt_ctx_;
    AVStream * stream_;


};
    
StreamMuxParser * NewStreamMuxParser(std::string codec_name);

#endif