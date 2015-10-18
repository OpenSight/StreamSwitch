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
 * stsw_ffmpeg_source.h
 *      FfmpegSource class header file, define intefaces of the FfmpegSource 
 * class. 
 *      FfmpegSource is a stream source implementation which acquire the 
 * media data through ffmpeg libavformat library. only support live stream 
 * by now
 * 
 * author: jamken
 * date: 2015-10-15
**/ 

#ifndef STSW_FFMPEG_SOURCE_H
#define STSW_FFMPEG_SOURCE_H

#include "stsw_ffmpeg_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>

#include <stream_switch.h>


typedef void (*OnErrorFun)(int error_code, void *user_data);

///////////////////////////////////////////////////////////////
//Type

class FfmpegSource:public stream_switch::SourceListener{
  
public:
    FfmpegSource();
    virtual ~FfmpegSource();
    int Init(std::string input, 
             std::string stream_name, 
             std::string ffmpeg_options,
             int local_gap_max_time, 
             int io_timeout,
             int source_tcp_port, 
             int queue_size, 
             int debug_flags);    

    void Uninit();
    int Start(OnErrorFun on_error, void *user_data);
    void Stop();


protected:
    virtual void OnKeyFrame(void);
    virtual void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic);    
       
protected: 


    stream_switch::StreamSource source_;    
    std::string input_name_;    
    uint32_t ssrc_; 
    int flags_;
    int io_timeout_;
    std::string ffmpeg_options_;

    
};
    

