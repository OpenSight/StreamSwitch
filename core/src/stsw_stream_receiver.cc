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
 * stsw_stream_receiver.cc
 *      StreamReceiver class implementation file, define all methods of StreamReceiver.
 * 
 * author: jamken
 * date: 2014-12-5
**/ 

#include <stsw_stream_receiver.h>
#include <stdint.h>
#include <list>
#include <string.h>
#include <errno.h>

#include <czmq.h>

#include <stsw_lock_guard.h>

#include <pb_packet.pb.h>
#include <pb_client_heartbeat.pb.h>
#include <pb_media.pb.h>
#include <pb_stream_info.pb.h>
#include <pb_metadata.pb.h>
#include <pb_media_statistic.pb.h>

namespace stream_switch {

StreamReceiver::StreamReceiver()
:last_api_socket_(NULL), client_hearbeat_socket_(NULL), subscriber_socket_(NULL),
worker_thread_id_(0), ssrc(0), flags_(0), 
last_heartbeat_time_(0), next_send_client_heartbeat_msec(0)
{
    
}


StreamReceiver::~StreamReceiver()
{
    
}

int StreamReceiver::InitBase(const StreamClientInfo &client_info, std::string *err_info)
{
    int ret;
    
    if(flags_ & STREAM_RECEIVER_FLAG_INIT != 0){
        SET_ERR_INFO(err_info, "Receiver already init");           
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
       
    //init socket 
    client_hearbeat_socket_ = zsock_new_req(api_addr.c_str());
    if(client_hearbeat_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new_req create api socket failed: maybe address error");   
        fprintf(stderr, "zsock_new_req create api socket failed: maybe address (%s) error", api_addr.c_str());
        goto error_2;            
    }
    zsock_set_linger(client_hearbeat_socket_, 0); //no linger   
    
  
    subscriber_socket_ = zsock_new_sub(subscriber_addr.c_str(), NULL);
    if(subscriber_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new_sub create publish socket failed: maybe address error");          
        fprintf(stderr, "zsock_new_sub create publish socket failed: "
                "maybe address (%s) error", subscriber_addr.c_str());
        goto error_2;            
    }    
    zsock_set_sndhwm(subscriber_socket_, STSW_SUBSCRIBE_SOCKET_HWM);  
    zsock_set_linger(subscriber_socket_, 0); //no linger   

    
    //init handlers
    RegisterSubHandler(PROTO_PACKET_CODE_MEDIA, 
                       STSW_PUBLISH_MEDIA_CHANNEL, 
                       (ReceiverSubHandler)StaticMediaFrameHandler, NULL);
 


    //init client info
    client_info_ = client_info;
    
    
    
    flags_ |= STREAM_RECEIVER_FLAG_INIT;        
   
    return 0;
    
error_2:

    if(subscriber_socket_ != NULL){
        zsock_destroy((zsock_t **)&subscriber_socket_);
        subscriber_socket_ = NULL;
        
    }
    
    if(client_hearbeat_socket_ != NULL){
        zsock_destroy((zsock_t **)&client_hearbeat_socket_);
        client_hearbeat_socket_ = NULL;
        
    }
error_1:    
    
    pthread_mutex_destroy(&lock_);   
    
error_0:
    
    return ret;
    
} 




void StreamReceiver::Uninit()
{
    if(!IsInit()){
        return; // no work
    }
    
    Stop(); // stop source first if it has not stop

    flags_ &= ~(STREAM_RECEIVER_FLAG_INIT); 

    UnregisterAllSubHandler();

    if(subscriber_socket_ != NULL){
        zsock_destroy((zsock_t **)&subscriber_socket_);
        subscriber_socket_ = NULL;
        
    }
    
    if(client_hearbeat_socket_ != NULL){
        zsock_destroy((zsock_t **)&client_hearbeat_socket_);
        client_hearbeat_socket_ = NULL;
        
    }
    
    if(last_api_socket_ != NULL){
        zsock_destroy((zsock_t **)&last_api_socket_);
        last_api_socket_ = NULL;
        
    }    
    
    pthread_mutex_destroy(&lock_);
    
    
}


bool StreamReceiver::IsInit()
{
    return (flags_ & STREAM_RECEIVER_FLAG_INIT) != 0;
}
bool StreamReceiver::IsMetaReady()
{
     return (flags_ & STREAM_RECEIVER_FLAG_META_READY) != 0;   
}

bool StreamReceiver::IsStarted()
{
     return (flags_ & STREAM_RECEIVER_FLAG_STARTED) != 0;   
}

StreamMetadata StreamReceiver::stream_meta()
{
    LockGuard guard(&lock_);  
    return stream_meta_;
}

StreamClientInfo StreamReceiver::client_info()
{
    LockGuard guard(&lock_);  
    return client_info_;    
}

void StreamReceiver::set_client_info(const StreamClientInfo &client_info)
{
    LockGuard guard(&lock_); 
    client_info_ = client_info;
}



int StreamReceiver::Start(std::string *err_info)
{
    if(!IsInit()){
        SET_ERR_INFO(err_info, "Receiver not init");          
        return -1;
    }
    
    LockGuard guard(&lock_);      

    if(flags_ & STREAM_RECEIVER_FLAG_STARTED){
        //already start
        return 0;        
    }
    if(worker_thread_id_ != 0){
        //STREAM_RECEIVER_FLAG_STARTED is clear but worker_thread_id_
        // is set, means other thread is stopping the receiver, just
        //return error
        SET_ERR_INFO(err_info, "An other thread is stopping the receiver, try later");       
        return ERROR_CODE_BUSY;
    }
    flags_ |= STREAM_RECEIVER_FLAG_STARTED;    
    
    //TODO(jamken): subscribe the channel key
    
    //start the internal thread
    int ret = pthread_create(&worker_thread_id_, NULL, ThreadRoutine, this);
    if(ret){
        if(err_info){
            *err_info = "pthread_create failed:";
            char tmp[64];
            //*err_info += strerror_r(errno, tmp, 64);
            *err_info += strerror(errno);
        }

        perror("Start Source internal thread failed");
        flags_ &= ~(STREAM_RECEIVER_FLAG_STARTED); 
        worker_thread_id_  = 0;
        return -1;
    }
    
    return 0;

}


void StreamReceiver::Stop()
{   
    
    pthread_mutex_lock(&lock_); 
    if(!(flags_ & STREAM_RECEIVER_FLAG_STARTED)){
        //not start
        pthread_mutex_unlock(&lock_); 
        return ;        
    }
    flags_ &= ~(STREAM_RECEIVER_FLAG_STARTED);
    

    //wait for the thread terminated
    if(worker_thread_id_ != 0){
        void * res;
        int ret;
        pthread_t worker_thread_id = worker_thread_id_;
        pthread_mutex_unlock(&lock_);  
        ret = pthread_join(worker_thread_id, &res);
        if (ret != 0){
            perror("Stop Receiver internal thread failed");
        }
        pthread_mutex_lock(&lock_); 
        worker_thread_id_ = 0;      
    }
    
    //TODO(jamken): unsubscribe the channel key
    
    
    
    pthread_mutex_unlock(&lock_);  
    
}







void StreamReceiver::RegisterSubHandler(int op_code, const char * channel_name, 
                                    ReceiverSubHandler handler, void * user_data)
{
    LockGuard guard(&lock_);   
    
    subsriber_handler_map_[op_code].channel_name = channel_name;
    subsriber_handler_map_[op_code].handler = handler;
    subsriber_handler_map_[op_code].user_data = user_data;    
}
void StreamReceiver::UnregisterSubHandler(int op_code)
{
    ReceiverSubHanderMap::iterator it;
    LockGuard guard(&lock_);   
    
    it = subsriber_handler_map_.find(op_code);
    if(it != subsriber_handler_map_.end()){
        subsriber_handler_map_.erase(it);
    }    
}
void StreamReceiver::UnregisterAllSubHandler()
{
    LockGuard guard(&lock_);   
    
    subsriber_handler_map_.clear();    
}      


int StreamReceiver::StaticMediaFrameHandler(StreamReceiver *receiver, const ProtoCommonPacket * msg, void * user_data)
{
    return receiver->MediaFrameHandler(msg, user_data);
}
int StreamReceiver::MediaFrameHandler(const ProtoCommonPacket * msg, void * user_data)
{
    return 0;
}

}





