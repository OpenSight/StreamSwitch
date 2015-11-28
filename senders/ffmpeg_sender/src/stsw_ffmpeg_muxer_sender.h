/**
 * This file is part of ffmpeg_sender, which belongs to StreamSwitch
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
 * stsw_ffmpeg_muxer_sender.h
 *      FfmpegMuxerSender class header file, define intefaces of the 
 * FfmpegMuxerSender class. 
 *      FfmpegMuxerSender is a stream sender implementation which acquire the 
 * media data from a StreamSwitch stream and send it to the ffmpeg muxing congtext. 
 * only support live stream by now
 * 
 * author: jamken
 * date: 2015-11-20
**/ 

#ifndef STSW_FFMPEG_MUXER_SENDER_H
#define STSW_FFMPEG_MUXER_SENDER_H


#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <stream_switch.h>



///////////////////////////////////////////////////////////////
//Type

class FFmpegMuxer;
class FFmpegMuxerSender: public stream_switch::SinkListener{
  
public:

	static FFmpegMuxerSender* Instance()
	{ 
		if ( NULL == s_instance )
		{
			s_instance = new FFmpegMuxerSender();
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

    virtual ~FFmpegMuxerSender();
    int Init(const std::string &dest_url, 
             const std::string &format,
             const std::string &ffmpeg_options_str,
             const std::string &stream_name, 
             const std::string &source_ip, int source_tcp_port, 
             unsigned long io_timeout,
             //uint32_t muxer_retry_count, 
             //uint32_t muxer_retry_interval, 
             uint32_t sub_queue_size,              
             uint32_t sink_debug_flags);    
    void Uninit();
    int Start();
    void Stop();
    
    int err_code();
    uint32_t frame_num();
    
    // stream_switch::SourceListener interface
    virtual void OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size);    
    
    virtual void OnMetadataMismatch(uint32_t mismatch_ssrc);  
 
       
protected: 
    FFmpegMuxerSender();
    static FFmpegMuxerSender * s_instance;

    stream_switch::StreamSink *sink_; 
    FFmpegMuxer * muxer_; 
    stream_switch::StreamMetadata meta_;
    int error_code_;
    
    //struct timespec init_ts_;  
    bool is_started_;

    std::string dest_url_;
    //std::string ffmpeg_options_str_;   
};
    

#endif