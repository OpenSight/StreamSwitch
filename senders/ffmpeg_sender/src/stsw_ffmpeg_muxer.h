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
 * stsw_ffmpeg_muxer.h
 *      FfmpegMuxer class header file, define intefaces of the FfmpegMuxer 
 * class. 
 *      FFmpegMuxer is a media file muxer based on ffmpeg libavformat 
 * 
 * author: jamken
 * date: 2015-11-22
**/ 

#ifndef STSW_FFMPEG_MUX_H
#define STSW_FFMPEG_MUX_H


#include <stdint.h>
#include <string>
#include <stream_switch.h>
#include <time.h>


extern "C"{

#include <libavutil/avutil.h>    
#include <libavformat/avformat.h>      
}

//class StreamMuxParser;
//class H264or5MuxParser;
//class Mpeg4MuxParser;
typedef std::vector<StreamMuxParser *> StreamMuxParserVector;


enum MuxerPlayMode{
    MUXER_PLAY_MODE_LIVE = 1,   
    MUXER_PLAY_MODE_REPLAY = 2,   
};


class FFmpegMuxer{
public:
    FFmpegMuxer();
    virtual ~FFmpegMuxer();
    virtual int Open(const std::string &dest_url, 
             const std::string &ffmpeg_options_str, 
             const stream_switch::StreamMetadata &metadata, 
             int io_timeout, 
             int play_mode);
    virtual void Close();
    virtual int WritePacket(const stream_switch::MediaFrameInfo &frame_info, 
                            const char * frame_data, 
                            size_t frame_size);
    
    
   
protected:
    //friend class StreamMuxParser;
    //friend class H264or5MuxParser;
    //friend class Mpeg4MuxParser;

    
    static int StaticIOInterruptCB(void* user_data);
    int IOInterruptCB();    
    virtual void StartIO();
    virtual void StopIO();
    
    AVFormatContext *fmt_ctx_;    
    struct timespec io_start_ts_;
    int io_timeout_;
    StreamMuxParserVector stream_parsers_;
};

#endif

