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
api_socket_(NULL), publish_socket_(NULL), lock_(NULL0, api_thread_(NULL), 
flags_(0), cur_bps_(0), 
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
        goto error_1;
        
    }    
    lock_ = (void *)new pthread_mutex_t;
    ret = pthread_mutex_init((pthread_mutex_t *)lock_, &attr);  
    if(ret){
        perror("pthread_mutexattr_settype failed");
        goto error_1;
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
    if(lock_ != NULL){
        pthread_mutex_destroy((pthread_mutex_t *)lock_);
        delete (pthread_mutex_t *)lock_;
    }
    
    pthread_mutexattr_destroy(&attr);  
    
error_0:
    
    return -1;
}


void StreamSource::Uninit()
{

}




bool StreamSource::IsInit()
{
    return (flags_ & STREAM_SOURCE_FLAG_INIT) != 0;
}
bool StreamSource::IsMetaReady()
{
     return (flags_ & STREAM_SOURCE_FLAG_META_READY) != 0;   
}


}





