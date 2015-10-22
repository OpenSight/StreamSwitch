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
 * stsw_ffmpeg_demuxer_source.h
 *      FfmpegDemuxerSource class header file, define intefaces of the 
 * FfmpegDemuxerSource class. 
 *      FfmpegDemuxerSource is a stream source implementation which acquire the 
 * media data through the ffmpeg libavformat library with demuxing. 
 * only support live stream by now
 * 
 * author: jamken
 * date: 2015-10-15
**/ 

#ifndef STSW_FFMPEG_DEMUXER_SOURCE_H
#define STSW_FFMPEG_DEMUXER_SOURCE_H


#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <pthread.h>

#include <stream_switch.h>

enum SourceMode{
    SOURCE_MODE_AUTO = 0,
    SOURCE_MODE_LIVE = 1,    
    SOURCE_MODE_REPLAY = 2, 
};

typedef void (*OnErrorFun)(int error_code, void *user_data);

///////////////////////////////////////////////////////////////
//Type

class FFmpegDemuxer;
class FFmpegDemuxerSource:public stream_switch::SourceListener{
  
public:
    FFmpegDemuxerSource();
    virtual ~FFmpegDemuxerSource();
    int Init(std::string input, 
             std::string stream_name, 
             std::string ffmpeg_options_str,
             int local_gap_max_time, 
             int io_timeout,
             bool native_frame_rate, 
             int source_tcp_port, 
             int queue_size, 
             int debug_flags);    

    void Uninit();
    int Start(OnErrorFun on_error_fun, void *user_data);
    void Stop();
    
    static void * StaticLiveThreadRoutine(void *arg);
    virtual void InternalLiveRoutine();  

protected:
    virtual void OnKeyFrame(void);
    virtual void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic);    
       
protected: 

    stream_switch::StreamSource source_;  
    stream_switch::StreamMetadata meta_;
    FFmpegDemuxer * demuxer_;
    pthread_t live_thread_id_;
    std::string input_name_;    
    int io_timeout_;
    std::string ffmpeg_options_str_;
 
    bool is_started_;
    bool native_frame_rate_; 
    OnErrorFun on_error_fun_; 
    void *user_data_;
    
};
    

