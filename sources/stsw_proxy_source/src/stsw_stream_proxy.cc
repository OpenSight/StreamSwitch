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
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>



#include <stream_switch.h>

#include <pthread.h>

#include "url.h"
#include "stsw_log.h"


///////////////////////////////////////////////////////////////
//Type



//////////////////////////////////////////////////////////////
//global variables



///////////////////////////////////////////////////////////////
//macro





///////////////////////////////////////////////////////////////
//functions

static int GetOutIp4(const char * dest_ip, uint16_t dest_port, char* buffer, size_t buflen) 
{
    if(dest_ip == NULL || buffer == NULL || buflen < 16){
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){
        perror("GetOutIp4 create socket failed");
        return -1;
    }


    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(dest_ip);
    serv.sin_port = htons(dest_port);

    int err = connect(sock, (const sockaddr*) &serv, sizeof(serv));
    if(err){
        perror("GetOutIp4 connect failed");
        close(sock);
        return -1;        
    }


    sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (sockaddr*) &name, &namelen);
    if(err){
        perror("GetOutIp4 getsocknameq failed");
        close(sock);
        return -1;        
    }    

    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, buflen);
    if(p == NULL){
        perror("GetOutIp4 inet_ntop failed");
        close(sock);
        return -1;            
    }
    
    close(sock);    
    return 0;
}

static std::string int2str(int int_value)
{
    std::stringstream stream;
    stream<<int_value;
    return stream.str();
}

////////////////////////////////////////////////////////////////
//class implementation


StreamProxySource* StreamProxySource::s_instance = NULL;

StreamProxySource::StreamProxySource()
:need_key_frame_(false), need_update_metadata_(false), 
last_frame_recv_(0), flags_(0)
{
    source_ = new stream_switch::StreamSource();
    sink_ = new stream_switch::StreamSink();
}
StreamProxySource::~StreamProxySource()
{
    //Uninit();
    
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
                            int debug_flags)  
{
    using namespace stream_switch;         
    int ret;
    std::string err_info;
    UrlParser url;
    StreamClientInfo client_info;
    char self_ip[32];
    
    
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
    
    if(GetOutIp4(url.host_name_.c_str(), url.port_, self_ip, 32) == 0){
        //successful get ip
        client_info.client_ip.assign(self_ip);
    }
    client_info.client_port = source_tcp_port;
    client_info.client_protocol = "stsw";
    pid_t pid = getpid() ;
    client_info.client_token = int2str(pid % 0xffffff);  
    client_info.client_text = "stsw_stream_proxy";
    
    
    //init lock
    pthread_mutexattr_t  attr;
    pthread_mutexattr_init(&attr);
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  
    if(ret){
        ret = ERROR_CODE_SYSTEM;
        perror("pthread_mutexattr_settype failed");   
        pthread_mutexattr_destroy(&attr);      
        goto error_0;
        
    }    
    ret = pthread_mutex_init(&lock_, &attr);  
    if(ret){
        ret = ERROR_CODE_SYSTEM;
        perror("pthread_mutex_init failed");
        pthread_mutexattr_destroy(&attr); 
        goto error_0;
    }
    pthread_mutexattr_destroy(&attr);    

    
    ret = sink_->InitRemote(url.host_name_, 
                            url.port_, 
                            client_info, 
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
    
    source_->set_stream_state(SOURCE_STREAM_STATE_CONNECTING);
    
    
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
    
    Stop();
        
    flags_ &= ~(STREAM_PROXY_FLAG_INIT);
    
    source_->Uninit();
    
    sink_->Uninit();
    
    pthread_mutex_destroy(&lock_);  

}


int StreamProxySource::UpdateStreamMetaData(int timeout, stream_switch::StreamMetadata * metadata)
{
    stream_switch::StreamMetadata tmp_metadata;
    int ret;
    std::string err_info;
    
    if(!IsInit()){
        return -1;
    }    
    ret = sink_->UpdateStreamMetaData(timeout, &tmp_metadata, &err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, "Get metadata from back-end source failed: %s\n", 
                   err_info.c_str());        
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR_CONNECT_FAIL);
        return ret;
    }
    
    if(tmp_metadata.play_type != stream_switch::STREAM_PLAY_TYPE_LIVE){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, "stsw_proxy_source only support relaying live stream\n");        
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        return -1;        
    }
    
    source_->set_stream_meta(tmp_metadata);
    
    if(metadata != NULL){
        *metadata = tmp_metadata;
    }
    
    flags_ |= STREAM_PROXY_FLAG_META_READY;
    

    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
              "StreamProxySource Start successful (stsw_url:%s, stream_name:%s)");    
    
    return 0;
                                               
}


int StreamProxySource::Start()
{
    int ret;
    std::string err_info;
    
    if(!IsInit()){
        return -1;
    }   
    if(IsStarted()){
        return 0;
    }
    
    
    
    
    last_frame_recv_ = time(NULL);
    
    ret = source_->Start(&err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, "Start internal source failed: %s\n", 
                   err_info.c_str());        
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        return ret;
    }
    
    //change source state
    source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_OK);    

    ret = sink_->Start(&err_info);
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, "Start internal sink failed: %s\n", 
                   err_info.c_str());        
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
        return ret;
    }
        

    
    flags_ |= STREAM_PROXY_FLAG_STARTED;
    
    return 0;
}
void StreamProxySource::Stop()
{
    if(!IsInit()){
        return;
    }   
    if(!IsStarted()){
        return;        
    }
    
    flags_ &= ~(STREAM_PROXY_FLAG_STARTED);
    
    sink_->Stop();
    source_->Stop();
    
}
#define MAX_FRAME_INTERVAL_SEC    10
#define DEFAULT_TIMEOUT_MSEC     5000

int StreamProxySource::Hearbeat()
{
    bool need_key_frame;
    bool need_update_metadata;    
    time_t last_frame_recv;    
    int ret = 0;
    std::string err_info;
    
    if(!IsInit()){
        return -1;
    }   
    if(!IsStarted()){
        return -1;        
    }
        
    
    //Get and reset the state data
    pthread_mutex_lock(&lock_); 
    need_key_frame = need_key_frame_;
    need_key_frame_ = false;
    need_update_metadata = need_update_metadata_;
    need_update_metadata_ = false;
    last_frame_recv = last_frame_recv_;    
    pthread_mutex_unlock(&lock_);     
    
    time_t now = time(NULL);
    
    if((now - last_frame_recv) >= MAX_FRAME_INTERVAL_SEC){
        source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR_MEIDA_STOP);
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, "stsw_proxy_source stop receiving media data\n");
        return stream_switch::ERROR_CODE_GENERAL;        
    }
    
    if(need_key_frame){
        ret = sink_->KeyFrame(DEFAULT_TIMEOUT_MSEC, &err_info);
        if(ret){
            source_->set_stream_state(stream_switch::SOURCE_STREAM_STATE_ERR);
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR, "Request key frame from back-end source failed:%s\n",
                   err_info.c_str());            
            return ret;
        }
    }
    
    if(need_update_metadata){
        Stop();
        ret = UpdateStreamMetaData(DEFAULT_TIMEOUT_MSEC, NULL);
        if(ret){
            return ret;
        }    
        ret = Start();
        if(ret){
            return ret;
        }    
    }
        
    return 0;
    

}


void StreamProxySource::OnKeyFrame(void)
{
    //no key frame
    pthread_mutex_lock(&lock_); 
    need_key_frame_ = true;    
    pthread_mutex_unlock(&lock_); 
}
void StreamProxySource::OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic)
{
    //use the default statistic info
    stream_switch::MediaStatisticInfo  sink_statistic;
    
    sink_->ReceiverStatistic(&sink_statistic);
    
    if(statistic->ssrc == sink_statistic.ssrc &&
       statistic->sub_streams.size() == sink_statistic.sub_streams.size()){
        
        //set the lost frame
        stream_switch::SubStreamMediaStatisticVector::iterator ita, itb;
        for(ita = statistic->sub_streams.begin(), 
            itb = sink_statistic.sub_streams.begin(); 
            ita != statistic->sub_streams.end();
            ita++, itb++){
            ita->lost_frames = itb->lost_frames;
        }        
        
    }
}

void StreamProxySource::OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                         const char * frame_data, 
                                         size_t frame_size)
{
    source_->SendLiveMediaFrame(frame_info, frame_data, frame_size, NULL);
    
    pthread_mutex_lock(&lock_); 
    last_frame_recv_ = time(NULL);    
    pthread_mutex_unlock(&lock_);                                              
}
                                         
void StreamProxySource::OnMetadataMismatch(uint32_t mismatch_ssrc)
{
    pthread_mutex_lock(&lock_); 
    need_update_metadata_ = true;    
    pthread_mutex_unlock(&lock_); 
}

