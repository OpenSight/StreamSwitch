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

#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>


#include "stsw_ffmpeg_source_global.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_demuxer.h"

static void sigusr1_handler (int signal_value)
{
    //nothig to do
}


FFmpegDemuxerSource * FFmpegDemuxerSource::s_instance = NULL;


FFmpegDemuxerSource::FFmpegDemuxerSource()
:live_thread_id_(0), io_timeout_(0), is_started_(false), 
native_frame_rate_(false),local_gap_max_time_(0), 
on_error_fun_(NULL), user_data_(NULL), default_stream_index_(0)
{
    //init the sigusr1_oldact_ field
    sigusr1_oldact_.sa_handler = SIG_DFL;
    sigusr1_oldact_.sa_flags = 0;
    sigemptyset (&sigusr1_oldact_.sa_mask);   

 
    // create demuxer object for this input type
    demuxer_ = new FFmpegDemuxer();
    
    source_ = new stream_switch::StreamSource();
}



FFmpegDemuxerSource::~FFmpegDemuxerSource()
{
    if(demuxer_ != NULL){
        delete demuxer_;
        demuxer_ = NULL;
    }
    
    if(source_ != NULL){
        delete source_;
        source_ = NULL;
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
    struct sigaction action;
    
    
    //printf("ffmpeg_options_str:%s\n", ffmpeg_options_str.c_str());
    //install the SIGUSR1 handler
    action.sa_handler = sigusr1_handler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    ret = sigaction (SIGUSR1, &action, &sigusr1_oldact_);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "sigaction failed: %s\n", 
                   strerror(errno));    
        ret = FFMPEG_SOURCE_ERR_GENERAL;
        goto error_out1;    
    }
     
    
    ret = source_->Init(stream_name, 
                        source_tcp_port, 
                        queue_size, 
                        this, 
                        debug_flags, 
                        &err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Init Source Failed (%d): %s\n", ret, err_info.c_str());  
        ret = FFMPEG_SOURCE_ERR_GENERAL;
        goto error_out2;
    }

    input_name_ = input;
    io_timeout_ = io_timeout;
    ffmpeg_options_str_ = ffmpeg_options_str;
    native_frame_rate_ = native_frame_rate;
    local_gap_max_time_ = local_gap_max_time;
    
    return 0;

error_out2:

    sigaction (SIGUSR1, &sigusr1_oldact_, NULL);
     
error_out1:     
    return ret;
}

void FFmpegDemuxerSource::Uninit()
{
    struct sigaction action;    
    
    input_name_.clear();
    ffmpeg_options_str_.clear();
    native_frame_rate_ = false;
    io_timeout_ = 0;
    local_gap_max_time_ = 0;
    
    
    //Uninit source
    source_->Uninit();

    //Uninstall the sig handler
    sigaction (SIGUSR1, &sigusr1_oldact_, NULL);    
    
}
int FFmpegDemuxerSource::Start(OnErrorFun on_error_fun, void *user_data)
{
    int ret;
    std::string err_info;
    int play_mode = PLAY_MODE_AUTO;
    
    if(is_started_){
        return 0; //already start
    }
    if(native_frame_rate_){
        play_mode = PLAY_MODE_LIVE; // for native_frame_rate enabled, force live mode
    }
    source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_CONNECTING);
    
    //open the demuxer
    demuxer_->set_io_enabled(true);    
    ret = demuxer_->Open(input_name_, 
                         ffmpeg_options_str_, 
                         io_timeout_, 
                         play_mode);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Demuxer open failed (ret: %d) for input: %s\n", 
                   ret, input_name_.c_str());  
 
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR_CONNECT_FAIL);
        goto err_out1;
    }

#define META_READ_TIMEOUT 10
        
    //read metadata from the demuxer
    ret = demuxer_->ReadMeta(&meta_, META_READ_TIMEOUT); 
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Demuxer ReadMeta failed (ret: %d) for intput:%s\n", 
                   ret, input_name_.c_str());   
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        goto err_out2;
    }    
    if(meta_.play_type == stream_switch::STREAM_PLAY_TYPE_REPLAY){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Cannot support non-live input without native_frame_rate enabled now\n");   
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        goto err_out2;        
    }
    
    //configure the metadata of soruce
    source_->set_stream_meta(meta_);
    
    //start the source
    ret = source_->Start(&err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Source Start failed (ret: %d):%s\n", 
                   ret, err_info.c_str());   
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        ret = FFMPEG_SOURCE_ERR_GENERAL;
        goto err_out2;
    }      
    
    source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_OK);
    
    on_error_fun_ = on_error_fun;
    user_data_ = user_data;
    is_started_ = true;    
    
    //create a thread to read packet
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
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        ret = -1;
        goto err_out3;
    }    
    
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
               "FFmpegDemuxerSource has started successful for input URL: %s\n", 
               input_name_.c_str());          
    return 0;    


err_out3:
    source_->Stop();

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
    
    demuxer_->set_io_enabled(false); //this can interrupt the current IO and prevent future IO
    
    // wait for the live working thread terminate
    if(live_thread_id_ != 0){
        void * res;
        int ret;
        //interrupt the live thread by SIGUSR1
        pthread_kill(live_thread_id_, SIGUSR1);
        ret = pthread_join(live_thread_id_, &res);
        if (ret != 0){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "join live Reading thread failed: %s\n", strerror(errno));            
        }
        live_thread_id_ = 0;      
    }
    
    on_error_fun_ = NULL;
    user_data_ = NULL;    
    
        
    // stop the source
    source_->Stop();
    
    // close demuxer
    demuxer_->set_io_enabled(true);
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

int FFmpegDemuxerSource::FindDefaultStreamIndex(const stream_switch::StreamMetadata &meta)
{
    int default_index = 0;
    for(int i=0; i< meta.sub_streams.size(); i++){
        if(meta.sub_streams[i].media_type == 
               stream_switch::SUB_STREAM_MEIDA_TYPE_VIDEO){
            default_index = i; //first video stream if exist
            break;
        }
    }
    return default_index;
}


void * FFmpegDemuxerSource::StaticLiveThreadRoutine(void *arg)
{
    FFmpegDemuxerSource * source = (FFmpegDemuxerSource * )arg;
    source->InternalLiveRoutine();
    return NULL;    
}


void FFmpegDemuxerSource::InternalLiveRoutine()
{
    //DemuxerPacket demuxer_packet; //holding media data;
    stream_switch::MediaFrameInfo frame_info;
    AVPacket pkt;
    int ret = 0;
    std::string err_info;
    struct timeval now;  
    bool is_meta_changed = false;
  
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;      
  
  
    while(is_started_){
        
        is_meta_changed = false;
        //read demuxer packet
        ret = demuxer_->ReadPacket(&frame_info, &pkt, &is_meta_changed); 
        if(ret){
            if(ret == FFMPEG_SOURCE_ERR_IO && !is_started_){
                //IO is interrupted by user because source has been stop, not a real error
                break;
            }else if(ret == FFMPEG_SOURCE_ERR_DROP){
                // dexumer need read again
                continue;
            }
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Demuxer Read packet error (%d)\n", ret);    
            source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR_MEIDA_STOP);
            break;
        }
        if(is_meta_changed){
            //read metadata from the demuxer
            ret = demuxer_->ReadMeta(&meta_, META_READ_TIMEOUT);
            if(ret){
                STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                           "Demuxer ReadMeta failed (ret: %d) for intput:%s\n", 
                           ret, input_name_.c_str());   
                source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
                break;
            }                 
            //update the metadata of soruce
            source_->set_stream_meta(meta_);            
        }
        
        // check local time gap
        if(local_gap_max_time_ > 0){
            gettimeofday(&now, NULL);    
            if(frame_info.timestamp.tv_sec <= (now.tv_sec - local_gap_max_time_) ||
               frame_info.timestamp.tv_sec >= (now.tv_sec + local_gap_max_time_)){
                //lost time sync between frame's time and local time 
                STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                        "Gap too much between local time(%lld.%06d) "
                        "and frame's time(%lld.%06d)\n",
                         (long long)now.tv_sec, (int)now.tv_usec,
                         (long long)frame_info.timestamp.tv_sec, 
                         (int)frame_info.timestamp.tv_usec);          
                source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR_TIME);
                break;
            } 
        }
        
        // check native_frame_rate
        if(native_frame_rate_ && 
           frame_info.sub_stream_index == default_stream_index_){

            while(now.tv_sec < frame_info.timestamp.tv_sec ||
                  (now.tv_sec == frame_info.timestamp.tv_sec &&
                   now.tv_usec < frame_info.timestamp.tv_usec)){
                
                if(!is_started_){
                    break;
                }
                
#define MAX_WAIT_US 10000 // 10 ms
                long long waittime = 
                    (frame_info.timestamp.tv_sec - now.tv_sec) * 1000000 
                    + (frame_info.timestamp.tv_usec - now.tv_usec);
                if(waittime < 0) {
                    waittime = 0;
                }else if (waittime > MAX_WAIT_US){
                    waittime = MAX_WAIT_US; //at most 10 ms
                }
                {
                    struct timespec req;
                    req.tv_sec = 0;
                    req.tv_nsec = waittime * 1000;            
                    nanosleep(&req, NULL);
                }                 
                
                gettimeofday(&now, NULL);             
            }
            
            if(!is_started_){
                break;
            }
            
        }        
        
        //send the media packet to source
        ret = source_->SendLiveMediaFrame(frame_info,
                                          (const char * )pkt.data,
                                          (size_t)pkt.size, 
                                          &err_info);
        if(ret){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                "Send live media frame Failed (%d):%s\n",
                 ret,  err_info.c_str());  
            source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
            ret = FFMPEG_SOURCE_ERR_GENERAL;
            break;            
        }
        av_free_packet(&pkt);

    }
    
    //free the demuxer packet for error 
    av_free_packet(&pkt);
 
    
    // callback for error condiction
    // note: if the source has alread stopped, don't invoke the user callback
    if(is_started_ && ret != 0){
        if(on_error_fun_){
            on_error_fun_(ret, user_data_);
        }        
    }
    
}

