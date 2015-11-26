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
#include <signal.h>

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

	static FFmpegDemuxerSource* Instance()
	{ 
		if ( NULL == s_instance )
		{
			s_instance = new FFmpegDemuxerSource();
		}

		return s_instance;
	}

	static void Uninstance()
	{
		if ( NULL != s_instance )
		{
			delete s_instance;
			s_instance = NULL;
		}
	}

    virtual ~FFmpegDemuxerSource();
    int Init(std::string input, 
             std::string stream_name, 
             std::string ffmpeg_options_str,
             int local_gap_max_time, 
             uint32_t io_timeout,
             bool native_frame_rate, 
             int source_tcp_port, 
             int queue_size, 
             int debug_flags);    

    void Uninit();
    int Start(OnErrorFun on_error_fun, void *user_data);
    void Stop();
    
    


protected:

    virtual int FindDefaultStreamIndex(const stream_switch::StreamMetadata &meta);
    
    static void * StaticLiveThreadRoutine(void *arg);
    virtual void InternalLiveRoutine();  

    virtual void OnKeyFrame(void);
    virtual void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic);    
       
protected: 
    FFmpegDemuxerSource();
    static FFmpegDemuxerSource * s_instance;

    stream_switch::StreamSource *source_; 
    FFmpegDemuxer * demuxer_; 
    stream_switch::StreamMetadata meta_;
 
    pthread_t live_thread_id_;
    std::string input_name_;    
    uint32_t io_timeout_;
    std::string ffmpeg_options_str_;
 
    bool is_started_;
    bool native_frame_rate_; 
    int local_gap_max_time_;
    OnErrorFun on_error_fun_; 
    void *user_data_;
    int default_stream_index_;
    struct sigaction sigusr1_oldact_;
   
};
    

#endif