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
 * stsw_ffmpeg_demuxer.h
 *      FfmpegDemuxer class header file, define intefaces of the FfmpegDemuxer 
 * class. 
 *      FFmpegDemuxer is a media file demuxer based on ffmpeg libavformat 
 * 
 * author: jamken
 * date: 2015-10-15
**/ 

#ifndef STSW_FFMPEG_DEMUXER_H
#define STSW_FFMPEG_DEMUXER_H


#include <stdint.h>
#include <string>
#include <stream_switch.h>
#include <time.h>


extern "C"{

#include <libavutil/avutil.h>    
#include <libavformat/avformat.h>      
}
struct PktNode{
    stream_switch::MediaFrameInfo frame_info;
    AVPacket pkt; 
};
class StreamParser;
class H264or5Parser;
class Mpeg4Parser;
typedef std::vector<StreamParser *> StreamParserVector;
typedef std::list<PktNode> PacketCachedList;

enum DemuxerPlayMode{
    PLAY_MODE_AUTO = 0, 
    PLAY_MODE_LIVE = 1,   
    PLAY_MODE_REPLAY = 2,   
};


class FFmpegDemuxer{
public:
    FFmpegDemuxer();
    virtual ~FFmpegDemuxer();
    int Open(const std::string &input, 
             const std::string &ffmpeg_options_str,
             int io_timeout, 
             int play_mode);
    void Close();
    int ReadPacket(stream_switch::MediaFrameInfo *frame_info, 
                   AVPacket *pkt, 
                   bool* is_meta_changed);
    int ReadMeta(stream_switch::StreamMetadata * meta, int timeout);
    
    virtual void set_io_enabled(bool io_enabled);
    virtual bool io_enabled();
    
   
protected:
    friend class StreamParser;
    friend class H264or5Parser;
    friend class Mpeg4Parser;
    
    virtual bool IsMetaReady();
    
    static int StaticIOInterruptCB(void* user_data);
    int IOInterruptCB();    
    virtual void StartIO();
    virtual void StopIO();
    
    
    bool io_enabled_;
    AVFormatContext *fmt_ctx_;    
    struct timespec io_start_ts_;
    int io_timeout_;
    stream_switch::StreamMetadata meta_;
    StreamParserVector stream_parsers_;
    PacketCachedList cached_pkts;
    uint32_t ssrc_;
};

#endif

