/**
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
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
 * stsw_stream_source.cc
 *      StreamSource class implementation file, define all methods of StreamSource.
 * 
 * author: jamken
 * date: 2014-11-8
**/ 

#include<stsw_stream_source.h>
#include<czmq.h>

#include<pb_packet.pb.h>

namespace stream_switch {

    
typedef std::list<ProtoClientHeartbeatReq> ClientHeartbeatList;    
    
struct ReceiversInfoType{
    ClientHeartbeatList receiver_list;
}    
    
    
    
    
StreamSource::StreamSource()
:tcp_port_(0), 
api_socket_(NULL), publish_socket_(NULL), api_thread_(0), 
thread_end_(true), flags_(0), cur_bytes(0), cur_bps_(0), 
last_frame_sec_(0), last_frame_usec_(0)， stream_state_(SOURCE_STREAM_STATE_CONNECTING), 
last_heartbeat_time(0),
{
    receivers_info_ = new ReceiversInfoType();
    
}

StreamSource::~StreamSource()
{
    if(receivers_info_ != NULL){
        delete receivers_info_;
    }
}

int StreamSource::Init(const std::string &stream_name, int tcp_port)
{
    int ret;
    //params check
    if(stream_name.size() == 0){
        return -1;
    }
    if(flags_ & STREAM_SOURCE_FLAG_INIT != 0){
        return -1;
    }
    
    stream_name_ = stream_name;
    tcp_port_ = tcp_port;
    
    //init lock
    pthread_mutexattr_t  attr;
    pthread_mutexattr_init(&attr);
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  
    if(ret){
        perror("pthread_mutexattr_settype failed");   
        pthread_mutexattr_destroy(&attr);      
        goto error_0;
        
    }    
    ret = pthread_mutex_init(&lock_, &attr);  
    if(ret){
        perror("pthread_mutexattr_settype failed");
        pthread_mutexattr_destroy(&attr); 
        goto error_0;
    }
    pthread_mutexattr_destroy(&attr);       
       
    //init socket 
    
    //init handlers
    RegisterApiHandler(0, StaticMetadataHandler, NULL);
    RegisterApiHandler(0, StaticKeyFrameHandler, NULL);    
    RegisterApiHandler(0, StaticStatisticHandler, NULL);
    RegisterApiHandler(0, StaticClientHeartbeatHandler, NULL);   

    //init metadata
    stream_meta_.sub_streams.clear();
    stream_meta_.bps = 0;
    stream_meta_.play_type = Stream_PLAY_TYPE_LIVE;
    stream_meta_.ssrc = 0;
    stream_meta_.source_proto = "";
    
    
    //init statistic 
    staitistic_.clear();
    
    flags_ |= STREAM_SOURCE_FLAG_INIT;        
   
    return 0;
    
error_2:

    if(api_socket_ != NULL){
        
    }
    
    if(publish_socket_ != NULL){
        
    }
error_1:    
    
    pthread_mutex_destroy(&lock_);
    

    
    
error_0:
    
    return -1;
}


void StreamSource::Uninit()
{
    if(!IsInit()){
        return; // no work
    }
    
    
}




bool StreamSource::IsInit()
{
    return (flags_ & STREAM_SOURCE_FLAG_INIT) != 0;
}
bool StreamSource::IsMetaReady()
{
     return (flags_ & STREAM_SOURCE_FLAG_META_READY) != 0;   
}



void StreamSource::set_stream_meta(const StreamMetadata & stream_meta)
{
    pthread_mutex_lock(&lock_); 
    
    //update meta
    stream_meta_ = stream_meta;       
    
    int sub_stream_num = stream_meta.sub_streams.size();
    
    
    // clear the statistic
    statistic_.clear();
    statistic_.resize(sub_stream_num);
    
    cur_bps_ = 0;
    cur_bytes = 0;
    last_frame_sec_ = 0;
    last_frame_usec_ = 0;
     
    
    flags_ |= STREAM_SOURCE_FLAG_INIT;  
    
    pthread_mutex_unlock(&lock_); 
    
}

void StreamSource::stream_meta(StreamMetadata * stream_meta)
{
    if(stream_meta != NULL){
        pthread_mutex_lock(&lock_); 
        *stream_meta = stream_meta_;
        pthread_mutex_unlock(&lock_); 
    }
}


int StreamSource::Start()
{
    if(!IsInit()){
        reutrn -1;
    }
    
    pthread_mutex_lock(&lock_); 
    if(!thread_end_){
        //already start
        pthread_mutex_unlock(&lock_); 
        return 0;        
    }
    thread_end_ = false;    
    pthread_mutex_unlock(&lock_); 
    
    //start the internal thread
    
}


void StreamSource::Stop()
{   
    
    pthread_mutex_lock(&lock_); 
    if(thread_end_){
        //already stop
        pthread_mutex_unlock(&lock_); 
        return ;        
    }
    thread_end_ = true;    
    pthread_mutex_unlock(&lock_);  

    //wait for the thread terminated
    
}


int StreamSource::SendMediaData(void)
{
    
}

void StreamSource::set_stream_state(int stream_state)
{
    if(!IsInit()){
        reutrn -1;
    }
    
    pthread_mutex_lock(&lock_); 
    int old_stream_state = stream_state_;
    stream_state_ = stream_state;
    if(old_stream_state != stream_state){
        //state change, send out a stream info msg at once
        SendStreamInfo();
    }
        
    pthread_mutex_unlock(&lock_);
    
}
int StreamSource::stream_state()
{
    return stream_state_;
}



}





