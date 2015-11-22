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
 * stsw_ffmpeg_muxer_sender.cc
 *      FfmpegMuxerSender class implementation file, includes all method 
 * implemenation of FfmpegMuxerSender class. 
 *      FfmpegMuxerSender is a stream sender implementation which acquire the 
 * media data from a StreamSwitch stream and send it to the ffmpeg muxing congtext. 
 * only support live stream by now
 * 
 * author: jamken
 * date: 2015-11-21
**/ 
#include "stsw_ffmpeg_muxer_sender.h"

#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>


#include "stsw_ffmpeg_sender_global.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_muxer.h"


FFmpegMuxerSender * FFmpegMuxerSender::s_instance = NULL;


FFmpegMuxerSender::FFmpegMuxerSender()
:error_code_(0), is_started_(false)
{

 
    // create demuxer object for this input type
    //demuxer_ = new FFmpegDemuxer();
    sink_ = new stream_switch::StreamSink();
}



FFmpegMuxerSender::~FFmpegMuxerSender()
{
    //if(demuxer_ != NULL){
    //    delete demuxer_;
    //    demuxer_ = NULL;
    //}
    
    if(sink_ != NULL){
        delete sink_;
        sink_ = NULL;
    }
}


int FFmpegMuxerSender::Init(const std::string &dest_url, 
                            const std::string &ffmpeg_options_str,
                            const std::string &stream_name, 
                            const std::string &source_ip, int source_tcp_port, 
                            unsigned long io_timeout,
                            //uint32_t muxer_retry_count, 
                            //uint32_t muxer_retry_interval, 
                            uint32_t sub_queue_size,              
                            uint32_t sink_debug_flags)
{
    std::string err_info;
    int ret = 0;
    uint32_t retry = muxer_retry_count;
    stream_switch::StreamClientInfo client_info;
    struct timespec sink_init_ts;  

    
    //check parameter;
    if(dest_url.size() == 0 || io_timeout <= 0){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "FFmpegMuxerSender Init() parameter error\n");        
        ret = FFMPEG_SENDER_ERR_GENERAL;
        goto error_out1;
    }
   
    //get client_info from muxer based on dest_url
    //??? 
    
    //init sink
    if(source_ip.size() != 0 && source_tcp_port !=0){
        ret = sink_->InitRemote(source_ip, source_tcp_port, 
                               client_info, 
                               sub_queue_size, 
                               this, 
                               sink_debug_flags,
                               &err_info);          
    }else if(stream_name.size() != 0){
        ret = sink_->InitLocal(stream_name, 
                               client_info, 
                               sub_queue_size, 
                               this, 
                               sink_debug_flags,
                               &err_info);  
    }else{
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "FFmpegMuxerSender Init() stream_name or source_ip/source_tcp_port absent");        
        ret = FFMPEG_SENDER_ERR_GENERAL;
        goto error_out1;      
    }
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Init Sink Failed (%d): %s\n", ret, err_info.c_str());  
        ret = FFMPEG_SOURCE_ERR_GENERAL;
        goto error_out1;
    }
    
    ret = sink_->UpdateStreamMetaData(io_timeout*1000, &meta_, &err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Update Sink metadata Failed (%d): %s\n", ret, err_info.c_str());  
        ret = FFMPEG_SOURCE_ERR_GENERAL;
        goto error_out2;
    }    
    clock_gettime(CLOCK_MONOTONIC, &sink_init_ts); 
    
    //open the muxer
    
    
#define MAX_SUBSCRIBER_BUFFER_TIME 4000     
    {
        struct timespec cur_ts;         
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);    
        uint64_t delta_ms = (cur_ts.tv_sec - sink_init_ts.tv_sec) * 1000 
                            + (cur_ts.tv_nsec - sink_init_ts.tv_nsec) / 1000000;
        if(delta_ms > MAX_SUBSCRIBER_BUFFER_TIME){
            sink_->DestroySubscriberSocket();
        }       
    }    
    
    dest_url_ = dest_url;
    error_code_ = 0;
    return 0;

error_out2:

    sink_->Uninit();
     
error_out1:    
    error_code_ = ret;
    return ret;
}

void FFmpegMuxerSender::Uninit()
{
    if(is_started_){
        Stop();
    }
    error_code_ = 0;
    //init_ts_.tv_nsec = init_ts_.tv_sec = 0;
    dest_url_.clear();
    
    //Uninit source
    sink_->Uninit(); 
    
}
int FFmpegMuxerSender::Start()
{
    int ret;
    std::string err_info;
        
    if(error_code_){
        return error_code_;
    }
    if(is_started_){
        return 0; //already start
    }
    is_started_ = true;
    
    ret = sink_->Start(&err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Sink Start failed (ret: %d): %s\n", 
                   ret, err_info.c_str());   

        goto err_out1;
    }    
    error_code_ = 0;    
    return 0;    
    
err_out1:
    is_started_ = false;
    error_code_ = ret;
    return ret;

    
}

void FFmpegMuxerSender::Stop()
{
    if(!is_started_){
        return; //already stop
    }
    is_started_ = false;
    
       
    // stop the sink
    sink_->Stop();   
}



int FFmpegMuxerSender::err_code()
{
    return err_code_;
}


uint32_t FFmpegMuxerSender::frame_num()
{
    return 0;
}

void FFmpegMuxerSender::OnLiveMediaFrame(const MediaFrameInfo &frame_info, 
                                         const char * frame_data, 
                                         size_t frame_size)
{
    if(error_code_){
        //if there is already some error, ignore the receiving frame
        return;
    }
    
    //send the frame to muxer
    
                                             
}
    
void FFmpegMuxerSender::OnMetadataMismatch(uint32_t mismatch_ssrc)
{
    error_code_ = FFMPEG_SENDER_ERR_METADATA_MISMATCH;
}
