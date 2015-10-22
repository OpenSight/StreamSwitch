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
 * stsw_ffmpeg_demuxer_source.cc
 *      FfmpegDemuxerSource class implementation file, includes all method 
 * implemenation of FfmpegDemuxerSource. 
 *      FfmpegDemuxerSource is a stream source implementation which acquire the 
 * media data through ffmpeg libavformat library with demuxing. 
 * only support live stream by now
 * 
 * author: jamken
 * date: 2015-10-18
**/ 
#include "stsw_ffmpeg_demuxer_source.h"


#include "stsw_ffmpeg_source_global.h"
#include "stsw_log.h"


FFmpegDemuxerSource::FFmpegDemuxerSource()
demuxer_(NULL), live_thread_id_(0), io_timeout_(0), is_started_(false), 
native_frame_rate_(false), on_error_fun_(NULL), user_data_(NULL)
{
    
}



FFmpegDemuxerSource::~FFmpegDemuxerSource()
{
    if(demuxer_ != NULL){
        delete demuxer_;
        demuxer_ = NULL;
    }
}


int FFmpegDemuxerSource::Init(std::string input, 
                              std::string stream_name, 
                              std::string ffmpeg_options_str,
                              int local_gap_max_time, 
                              int io_timeout,
                              bool native_frame_rate, 
                              int source_tcp_port, 
                              int queue_size, 
                              int debug_flags)
{
    std::string err_info;
    int ret = 0;
    
    input_name_ = input;
    io_timeout_ = io_timeout;
    ffmpeg_options_str_ = ffmpeg_options_str;
    native_frame_rate_ = native_frame_rate;
     
    // create demuxer object for this input type
    demuxer_ = new FFmpegDemuxer();
     
    ret = source_.Init(stream_name, 
                       source_tcp_port, 
                       queue_size, 
                       this, 
                       debug_flags, 
                       &err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Init Source Failed: %s\n", err_info.c_str());   
        goto error_out1:
    }
    
    return 0;
     
error_out1:     
     return ret;
}

void FFmpegDemuxerSource::Uninit()
{
    if(demuxer_ != NULL){
        delete demuxer_;
        demuxer_ = NULL;
    }   

    
}
int FFmpegDemuxerSource::Start(OnErrorFun on_error_fun, void *user_data)
{
    int ret;
    std::string err_info;
    
    if(is_started_){
        return; //already start
    }
    
    //open the demuxer
    ret = demuxer_->Open(input_name_, 
                         ffmpeg_options_str_, 
                         io_timeout_);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Demuxer open failed (ret: %d) for input: %s\n", 
                   ret, input_name_.c_str());   
        goto err_out1;
    }    
        
    //read metadata from the demuxer
    ret = demuxer_->ReadMeta(&meta_);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Demuxer ReadMeta failed (ret: %d) for intput:%s\n", 
                   ret, input_name_.c_str());   
        goto err_out2;
    }     
    
    //configure the metadata of soruce
    source_.set_stream_meta(meta_);
    
    //start the source
    ret = source_.Start(&err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Source ReadMeta failed (ret: %d) for demuxer(%s)\n", 
                   ret, input_name_.c_str());   
        goto err_out2;
    }      
    
    //create a thread to read packet
    on_error_fun_ = on_error_fun;
    user_data_ = user_data;
    is_started_ = true;
    ret = pthread_create(&live_thread_id_, NULL, 
                         FFmpegDemuxerSource::StaticLiveThreadRoutine, 
                         this);
    if(ret){
        
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "pthread_create live reading thread failed: %s\n", 
                   strerror(errno));         
        
        is_started_ = false;
        live_thread_id_  = 0;
        on_error_fun_ = NULL;
        user_data_ = NULL;
        goto error_out3;

    }    
    
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
               "FFmpegDemuxerSource has started successful for input URL: %s\n", 
                input_name_.c_str());          
    return 0;    


err_out3:
    source_.Stop();

err_out2:
    demuxer_->Close();
    
err_out1:
    return ret;

    
}

void FFmpegDemuxerSource::Stop()
{
    if(!is_started_){
        return; //already stop
    }
    is_started_ = false;
    
    // wait for the live working thread terminate
    //wait for the thread terminated
    if(live_thread_id_ != 0){
        void * res;
        int ret;
        pthread_t live_thread_id = live_thread_id_;
        ret = pthread_join(live_thread_id, &res);
        if (ret != 0){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Stop live Reading thread failed: %s\n", strerror(errno));            
        }
        live_thread_id_ = 0;      
    }
    
    on_error_fun_ = NULL;
    user_data_ = NULL;    
    
        
    // stop the source
    source_.Stop();
    
    // close demuxer
    demuxer_->Close();    
    
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
               "FFmpegDemuxerSource has stopped for input URL: %s\n", 
                input_name_.c_str());       
    
}


void FFmpegDemuxerSource::OnKeyFrame(void)
{
    // nothing to do     
}

void FFmpegDemuxerSource::OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic)
{
    // nothing to do 
}

void * FFmpegDemuxerSource::StaticLiveThreadRoutine(void *arg)
{
    FFmpegDemuxerSource * source = (StreamSource * )arg;
    source->InternalLiveRoutine();
    return NULL;    
}


void FFmpegDemuxerSource::InternalLiveRoutine()
{
    DemuxerPacket demuxer_packet; //holding media data;
    int ret;
    std::string err_info;
    
    while(is_started_){
        ret = demuxer_->ReadPacket(&demuxer_packet);
        if(ret){
            
        }
        //send the media packet to source
        ret = source_->SendLiveMediaFrame(demuxer_packet.frame_info,
                                          demuxer_packet.data,
                                          demuxer_packet.size, 
                                          &err_info);
        if(ret){
            
        }
        
        demuxer_->FreePacket(&demuxer_packet);        
    }
    
}

