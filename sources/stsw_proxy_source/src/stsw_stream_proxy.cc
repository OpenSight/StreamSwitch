/**
 * This file is part of stsw_proxy_source, which belongs to StreamSwitch
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
 * stsw_stream_proxy.cc
 *      the implementation of StreamProxySource class, includes 
 * all the definition of its methods.
 * 
 * author: jamken
 * date: 2015-6-23
**/ 

#include "stsw_stream_proxy.h"


#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>

#include <stream_switch.h>

#include <pthread.h>

#include "url.h"

///////////////////////////////////////////////////////////////
//Type



//////////////////////////////////////////////////////////////
//global variables



///////////////////////////////////////////////////////////////
//macro





///////////////////////////////////////////////////////////////
//functions
    


////////////////////////////////////////////////////////////////
//class implementation

StreamProxySource::StreamProxySource()
:need_key_frame_(false), need_update_metadata_(false), 
last_frame_recv_(0), flags_(0)
{
    source_ = new stream_switch::StreamSource();
    sink_ = new stream_switch::StreamSink():
}
StreamProxySource::~StreamProxySource()
{
    Uninit();
    
    delete sink_;

    delete source_;
    
}

bool StreamProxySource::IsInit()
{
    return (flags_ & STREAM_PROXY_FLAG_INIT) != 0;
}

bool StreamProxySource::IsStarted()
{
     return (flags_ & STREAM_PROXY_FLAG_STARTED) != 0;   
}
bool StreamProxySource::IsMetaReady()
{
    return (flags_ & STREAM_PROXY_FLAG_META_READY) != 0;   
}



int StreamProxySource::Init(std::string stsw_url,
                            std::string stream_name,                       
                            int source_tcp_port,             
                            int sub_queue_size, 
                            int pub_queue_size,
                            int debug_flags);    
{
    using namespace stream_switch;         
    int ret;
    std::string err_info;
    UrlParser url;
    StreamClientInfo client_info;
    
    
    if(stsw_url.size() == 0){
        STDERR_LOG(LOG_LEVEL_ERR, "stsw_url cannot be empty\n");
        return -1;
    }else if(sub_queue_size == 0 || pub_queue_size == 0){
        STDERR_LOG(LOG_LEVEL_ERR, "sub_queue_size/pub_queue_size cannot be 0\n");
        return -1;        
    }
    url.Parse(stsw_url.c_str());
    if(url.protocol_.size() == 0 ||
       url.protocol_ != "stsw"){
        STDERR_LOG(LOG_LEVEL_ERR, "URL invalid: protocol must be stsw\n");
        return -1;            
    }
    if(url.host_name_.size() == 0){
        STDERR_LOG(LOG_LEVEL_ERR, "URL invalid: host absent\n");
        return -1;            
    }
    if(url.port_ == 0){
        STDERR_LOG(LOG_LEVEL_ERR, "URL invalid: port must be given\n");
        return -1;        
    }
    
    if(IsInit()){
        return -1;
    }
    
    //init lock
    pthread_mutexattr_t  attr;
    pthread_mutexattr_init(&attr);
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  
    if(ret){
        SET_ERR_INFO(err_info, "pthread_mutexattr_settype failed");   
        ret = ERROR_CODE_SYSTEM;
        perror("pthread_mutexattr_settype failed");   
        pthread_mutexattr_destroy(&attr);      
        goto error_0;
        
    }    
    ret = pthread_mutex_init(&lock_, &attr);  
    if(ret){
        SET_ERR_INFO(err_info, "pthread_mutex_init failed"); 
        ret = ERROR_CODE_SYSTEM;
        perror("pthread_mutex_init failed");
        pthread_mutexattr_destroy(&attr); 
        goto error_0;
    }
    pthread_mutexattr_destroy(&attr);    

    
    ret = sink_->InitRemote(url.host_name_, 
                            url.port_, 
                            &client_info, 
                            sub_queue_size, 
                            this,
                            debug_flags, 
                            &err_info);
    if(ret){
        STDERR_LOG(LOG_LEVEL_ERR, "Init sink failed: %s\n", 
                   err_info.c_str());
        goto error_1;        
    }
    
    
    ret = source_->Init(stream_name, 
                        source_tcp_port,
                        pub_queue_size,
                        this,
                        debug_flags,
                        &err_info);
    if(ret){
        STDERR_LOG(LOG_LEVEL_ERR, "Init source failed: %s\n", 
                   err_info.c_str());
        goto error_2;             
    }
    
    
    flags_ |= STREAM_PROXY_FLAG_INIT;
    
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
              "StreamProxySource Init successful (stsw_url:%s, stream_name:%s)", 
              stsw_url.c_str(), stream_name.c_str());    


    return 0;
    
    
    
error_2:
    sink_->Uninit();
    
error_1:
    pthread_mutex_destroy(&lock_);   
    
error_0:
    return ret;
}

void StreamProxySource::Uninit()
{
    
    if(!IsInit()){
        return;
    }
        
    flags_ &= ~(STREAM_PROXY_FLAG_INIT);
    
    source_->Uninit();
    
    sink_->Uninit();
    
    pthread_mutex_destroy(&lock_);  

}


int StreamProxySource:UpdateStreamMetaData(int timeout, 
                                           stream_switch::StreamMetadata * metadata, 
                                           std::string *err_info)
{
    return 0;
                                               
}


int StreamProxySource::Start()
{
    return 0;
}
void StreamProxySource::Stop()
{

}

int StreamProxySource::Hearbeat()
{
    return 0;
}


void StreamProxySource::OnKeyFrame(void)
{
    //no key frame
}
void StreamProxySource::OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic)
{
    //use the default statistic info
}

void StreamProxySource::OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                         const char * frame_data, 
                                         size_t frame_size)
{
                                             
}
                                         
void StreamProxySource::OnMetadataMismatch(uint32_t mismatch_ssrc)
{
    
}

