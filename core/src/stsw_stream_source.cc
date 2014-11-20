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
#include<stdint.h>
#include<list>

#include<czmq.h>


#include<pb_packet.pb.h>
#include<pb_client_heartbeat.pb.h>

namespace stream_switch {

    
typedef std::list<ProtoClientHeartbeatReq> ClientHeartbeatList;    
    
struct ReceiversInfoType{
    ClientHeartbeatList receiver_list;
};    
    
    
    
    
StreamSource::StreamSource()
:tcp_port_(0), 
api_socket_(NULL), publish_socket_(NULL), api_thread_id_(0), 
thread_end_(true), flags_(0), cur_bytes(0), cur_bps_(0), 
last_frame_sec_(0), last_frame_usec_(0), stream_state_(SOURCE_STREAM_STATE_CONNECTING), 
last_heartbeat_time_(0)
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
#define MAX_SOCKET_BIND_ADDR_LEN 255    

    char tmp_addr[MAX_SOCKET_BIND_ADDR_LEN + 1];
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);
    if(tcp_port == 0){
        //no tcp address
        int ret;
        ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                       "@ipc://@%s/%s/%s", 
                       STSW_SOCKET_NAME_STREAM_PREFIX, 
                       stream_name.c_str(), 
                       STSW_SOCKET_NAME_STREAM_API);
        if(ret == MAX_SOCKET_BIND_ADDR_LEN){
            fprintf(stderr, "socket address too long: %s", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }else{
        //tcp address exist
        int ret;
        ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                       "@ipc://@%s/%s/%s,@tcp://*:%d", 
                       STSW_SOCKET_NAME_STREAM_PREFIX, 
                       stream_name.c_str(), 
                       STSW_SOCKET_NAME_STREAM_API, 
                       tcp_port);
        if(ret == MAX_SOCKET_BIND_ADDR_LEN){
            fprintf(stderr, "socket address too long: %s", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }       
    api_socket_ = zsock_new_rep(tmp_addr);
    if(api_socket_ == NULL){
        fprintf(stderr, "zsock_new_rep create publish socket failed: maybe address conflict", tmp_addr);
        goto error_2;            
    }
    zsock_set_linger(api_socket_, 0); //no linger
    
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);
    if(tcp_port == 0){
        //no tcp address
        int ret;
        ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                       "@ipc://@%s/%s/%s", 
                       STSW_SOCKET_NAME_STREAM_PREFIX, 
                       stream_name.c_str(), 
                       STSW_SOCKET_NAME_STREAM_PUBLISH);
        if(ret == MAX_SOCKET_BIND_ADDR_LEN){
            fprintf(stderr, "socket address too long: %s", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }else{
        //tcp address exist
        int ret;
        ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                       "@ipc://@%s/%s/%s,@tcp://*:%d", 
                       STSW_SOCKET_NAME_STREAM_PREFIX, 
                       stream_name.c_str(), 
                       STSW_SOCKET_NAME_STREAM_PUBLISH, 
                       tcp_port + 1);
        if(ret == MAX_SOCKET_BIND_ADDR_LEN){
            fprintf(stderr, "socket address too long: %s", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }    
    publish_socket_ = zsock_new_pub(tmp_addr);
    if(api_socket_ == NULL){
        fprintf(stderr, "zsock_new_pub create publish socket failed: "
                "maybe address conflict", tmp_addr);
        goto error_1;            
    }    
    //wait for 100 ms to send the remaining packet before close
    zsock_set_linger(publish_socket_, STSW_PUBLISH_SOCKET_LINGER);
 
    zsock_set_sndhwm(publish_socket_, STSW_PUBLISH_SOCKET_HWM);   

    
    //init handlers
    RegisterApiHandler(PROTO_PACKET_CODE_METADATA, (SourceApiHandler)StaticMetadataHandler, NULL);
    RegisterApiHandler(0, (SourceApiHandler)StaticKeyFrameHandler, NULL);    
    RegisterApiHandler(0, (SourceApiHandler)StaticStatisticHandler, NULL);
    RegisterApiHandler(0, (SourceApiHandler)StaticClientHeartbeatHandler, NULL);   

    //init metadata
    stream_meta_.sub_streams.clear();
    stream_meta_.bps = 0;
    stream_meta_.play_type = Stream_PLAY_TYPE_LIVE;
    stream_meta_.ssrc = 0;
    stream_meta_.source_proto = "";    
    
    //init statistic 
    statistic_.clear();
    
    flags_ |= STREAM_SOURCE_FLAG_INIT;        
   
    return 0;
    
error_2:

    if(api_socket_ != NULL){
        zsock_destroy((zsock_t **)&api_socket_);
        api_socket_ = NULL;
        
    }
    
    if(publish_socket_ != NULL){
        zsock_destroy((zsock_t **)&publish_socket_);
        publish_socket_ = NULL;
        
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
    
    

    Stop(); // stop source first if it has not stop

    flags_ &= ~(STREAM_SOURCE_FLAG_INIT); 

    UnregisterAllApiHandler();

    if(api_socket_ != NULL){
        zsock_destroy((zsock_t **)&api_socket_);
        api_socket_ = NULL;
        
    }
    
    if(publish_socket_ != NULL){
        zsock_destroy((zsock_t **)&publish_socket_);
        publish_socket_ = NULL;
        
    }
    
    pthread_mutex_destroy(&lock_);
    
    
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
     
    
    flags_ |= STREAM_SOURCE_FLAG_META_READY;  
    
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
        return -1;
    }
    
    pthread_mutex_lock(&lock_); 
    if(flags_ & STREAM_SOURCE_FLAG_STARTED){
        //already start
        pthread_mutex_unlock(&lock_); 
        return 0;        
    }
    flags_ |= STREAM_SOURCE_FLAG_STARTED;    
    pthread_mutex_unlock(&lock_);     
    
    //start the internal thread
    int ret = pthread_create(&api_thread_id_, NULL, ThreadRoutine, this);
    if(ret){
        perror("Start Source internal thread failed");
        pthread_mutex_lock(&lock_); 
        flags_ &= ~(STREAM_SOURCE_FLAG_STARTED); 
        api_thread_id_  = 0;
        pthread_mutex_unlock(&lock_);  
        return -1;
    }

}


void StreamSource::Stop()
{   
    
    pthread_mutex_lock(&lock_); 
    if(!(flags_ & STREAM_SOURCE_FLAG_STARTED)){
        //not start
        pthread_mutex_unlock(&lock_); 
        return ;        
    }
    flags_ &= ~(STREAM_SOURCE_FLAG_STARTED);
    pthread_mutex_unlock(&lock_);  

    //wait for the thread terminated
    if(api_thread_id_ != 0){
        void * res;
        int ret;
        ret = pthread_join(api_thread_id_, &res);
        if (ret != 0){
            perror("Stop Source internal thread failed");
        }
        api_thread_id_ = 0;      
    }
    
}


int StreamSource::SendMediaData(int32_t sub_stream_index, 
                                uint64_t frame_seq,     
                                MediaFrameType frame_type,
                                const struct timeval &timestamp,                               
                                uint32_t ssrc, 
                                std::string data)
{
    return 0;
}


void StreamSource::set_stream_state(int stream_state)
{
    if(!IsInit()){
        return;
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


void StreamSource::RegisterApiHandler(int op_code, SourceApiHandler handler, void * user_data)
{
    pthread_mutex_lock(&lock_); 
    
    api_handler_map_[op_code].handler = handler;
    api_handler_map_[op_code].user_data = user_data;
    
    pthread_mutex_unlock(&lock_);
    
}
void StreamSource::UnregisterApiHandler(int op_code)
{
    SourceApiHanderMap::iterator it;
    pthread_mutex_lock(&lock_); 
    
    it = api_handler_map_.find(op_code);
    if(it != api_handler_map_.end()){
        api_handler_map_.erase(it);
    }
   
    pthread_mutex_unlock(&lock_);   
}
void StreamSource::UnregisterAllApiHandler()
{
    pthread_mutex_lock(&lock_); 
    
    api_handler_map_.clear();
   
    pthread_mutex_unlock(&lock_);     
}


void StreamSource::KeyFrame(void)
{
    //default nothing to do
    //need child class to implement this function
}



int StreamSource::StaticMetadataHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return source->MetadataHandler(request, reply, user_data);    
}
int StreamSource::StaticKeyFrameHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return source->KeyFrameHandler(request, reply, user_data);
}
int StreamSource::StaticStatisticHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return source->StatisticHandler(request, reply, user_data);
}
int StreamSource::StaticClientHeartbeatHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return source->ClientHeartbeatHandler(request, reply, user_data);
}
    
int StreamSource::MetadataHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return 0;
}
int StreamSource::KeyFrameHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return 0;
}
int StreamSource::StatisticHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return 0;
}
int StreamSource::ClientHeartbeatHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    return 0;
}


int StreamSource::RpcHandler()
{
    zframe_t * in_frame = NULL;

    in_frame = zframe_recv(api_socket_);
    if(in_frame == NULL){
        return 0;  // no frame receive
    }
    std::string in_data((const char *)zframe_data(in_frame), zframe_size(in_frame));
    ProtoCommonPacket request;
    ProtoCommonPacket reply;
    
    // initialize the reply;
    
    
    if(request.ParseFromString(in_data)){
        
        
    }

    //after used, need free the frame
    zframe_destroy(&in_frame);

    
    // send back the reply
    std::string out_data;
    reply.SerializeToString(&out_data);
    zframe_t * out_frame = NULL;
    out_frame = zframe_new(out_data.data(), out_data.size());
    zframe_send(&out_frame, api_socket_, ZFRAME_DONTWAIT);
    
    
    

    
    return 0;   
}


int StreamSource::Heartbeat(int64_t now)
{
    if(now == 0){
        now = zclock_mono();
    }
    
    

}
    
void * StreamSource::ThreadRoutine(void * arg)
{
    StreamSource * source = (StreamSource * )arg;
    zpoller_t  * poller =zpoller_new (source->api_socket_);
    int64_t next_heartbeat_time = zclock_mono() + 
        STSW_STREAM_SOURCE_HEARTBEAT_INT;
    
#define MAX_POLLER_WAIT_TIME    100
    
    while(source->flags_ & STREAM_SOURCE_FLAG_STARTED){
        int64_t now = zclock_mono();
        
        //calculate the timeout
        int timeout = next_heartbeat_time - now;
        if(timeout > MAX_POLLER_WAIT_TIME){
            timeout = MAX_POLLER_WAIT_TIME;
        }else if (timeout < 0){
            timeout = 0;  //if timeout is nagative, zpoller_wait would block 
                          //until the socket is ready to read
        }        
        
        // check for api socket read event
        void * socket =  zpoller_wait(poller, timeout);  //wait for timeout
        if(socket != NULL){
            source->RpcHandler();
        }
             
        
        // check for heartbeat
        now = zclock_mono();
        if(now >= next_heartbeat_time){            
            source->Heartbeat(now);
            
            //calculate next heartbeat time
            do{
                next_heartbeat_time += STSW_STREAM_SOURCE_HEARTBEAT_INT;
            }while(next_heartbeat_time <= now);            
        } 
        
    }
    
    if(poller != NULL){
        zpoller_destroy (&poller);
    }
        
    return NULL;
}
    
    
int SendStreamInfo(void)
{
    return 0;
}



}





