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
#include <set>
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
worker_thread_id_(0), ssrc_(0), flags_(0), 
last_heartbeat_time_(0), 
last_send_client_heartbeat_msec_(0),
next_send_client_heartbeat_msec_(0)
{
    
}


StreamReceiver::~StreamReceiver()
{
    
}

int StreamReceiver::InitRemote(const std::string &source_ip, int source_tcp_port, 
                               const StreamClientInfo &client_info,
                               std::string *err_info)
{
    int ret;
    
    //generate api socket address

#define MAX_SOCKET_BIND_ADDR_LEN 255    

    char tmp_addr[MAX_SOCKET_BIND_ADDR_LEN + 1];
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);

    ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                   ">tcp://%s:%d",  source_ip.c_str(), source_tcp_port);

    if(ret == MAX_SOCKET_BIND_ADDR_LEN){
        ret = ERROR_CODE_PARAM;
        fprintf(stderr, "socket address too long: %s", tmp_addr);
        SET_ERR_INFO(err_info, "socket address too long");
        goto error_out;            
    }else if(ret < 0){
        ret = ERROR_CODE_SYSTEM;
        perror("snprintf for socket address failed");
        SET_ERR_INFO(err_info, "snprintf for socket address failed");
        goto error_out;              
    }
    api_addr_ = tmp_addr;
        
    
    //generate sub socket address
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);

    ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                   ">tcp://%s:%d",  source_ip.c_str(), source_tcp_port + 1);

    if(ret == MAX_SOCKET_BIND_ADDR_LEN){
        ret = ERROR_CODE_PARAM;
        fprintf(stderr, "socket address too long: %s", tmp_addr);
        SET_ERR_INFO(err_info, "socket address too long");
        goto error_out;            
    }else if(ret < 0){
        ret = ERROR_CODE_SYSTEM;
        perror("snprintf for socket address failed");
        SET_ERR_INFO(err_info, "snprintf for socket address failed");
        goto error_out;              
    }
    subscriber_addr_ = tmp_addr;
    
    return InitBase(client_info, err_info);

error_out:
    api_addr_.clear();
    subscriber_addr_.clear();
    
    return ret;
    
    
}
int StreamReceiver::InitLocal(const std::string &stream_name, 
                          const StreamClientInfo &client_info,
                          std::string *err_info)
{
    int ret;
    
    //generate api socket address

#define MAX_SOCKET_BIND_ADDR_LEN 255    

    char tmp_addr[MAX_SOCKET_BIND_ADDR_LEN + 1];
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);

    ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                   ">ipc://@%s/%s/%s",  
                   STSW_SOCKET_NAME_STREAM_PREFIX, 
                   stream_name.c_str(), 
                   STSW_SOCKET_NAME_STREAM_API);

    if(ret == MAX_SOCKET_BIND_ADDR_LEN){
        ret = ERROR_CODE_PARAM;
        fprintf(stderr, "socket address too long: %s", tmp_addr);
        SET_ERR_INFO(err_info, "socket address too long");
        goto error_out;            
    }else if(ret < 0){
        ret = ERROR_CODE_SYSTEM;
        perror("snprintf for socket address failed");
        SET_ERR_INFO(err_info, "snprintf for socket address failed");
        goto error_out;              
    }
    api_addr_ = tmp_addr;
        
    
    //generate sub socket address
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);

    ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                   ">ipc://@%s/%s/%s",  
                   STSW_SOCKET_NAME_STREAM_PREFIX, 
                   stream_name.c_str(), 
                   STSW_SOCKET_NAME_STREAM_PUBLISH);

    if(ret == MAX_SOCKET_BIND_ADDR_LEN){
        ret = ERROR_CODE_PARAM;
        fprintf(stderr, "socket address too long: %s", tmp_addr);
        SET_ERR_INFO(err_info, "socket address too long");
        goto error_out;            
    }else if(ret < 0){
        ret = ERROR_CODE_SYSTEM;
        perror("snprintf for socket address failed");
        SET_ERR_INFO(err_info, "snprintf for socket address failed");
        goto error_out;              
    }
    subscriber_addr_ = tmp_addr;
    
    ret = InitBase(client_info, err_info);
    if(ret){
        goto error_out;
    }

error_out:
    api_addr_.clear();
    subscriber_addr_.clear();
    
    return ret;


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
    client_hearbeat_socket_ = zsock_new_req(api_addr_.c_str());
    if(client_hearbeat_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new_req create api socket failed: maybe address error");   
        fprintf(stderr, "zsock_new_req create api socket failed: maybe address (%s) error", api_addr_.c_str());
        goto error_2;            
    }
    zsock_set_linger(client_hearbeat_socket_, 0); //no linger   
    

    
    //init handlers
    RegisterSubHandler(PROTO_PACKET_CODE_MEDIA, 
                       STSW_PUBLISH_MEDIA_CHANNEL, 
                       (ReceiverSubHandler)StaticMediaFrameHandler, NULL);
 


    //init client info
    client_info_ = client_info;
    
    
    
    flags_ |= STREAM_RECEIVER_FLAG_INIT;        
   
    return 0;
    
error_2:


    
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
    
    //
    // create a new subscriber socket
    // moving subscribe socket from init() to start() is for cleanning 
    // the socket context to avoid receiving a overdue broadcast 
    // frame. If create subscrber socket in init() ,and keep the same 
    // subscribe socket for the whole life time of receiver. The receiver
    // may get a overdue frame ater next time start()
    subscriber_socket_ = zsock_new_sub(subscriber_addr_.c_str(), NULL);
    if(subscriber_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new_sub create publish socket failed: maybe address error");          
        fprintf(stderr, "zsock_new_sub create publish socket failed: "
                "maybe address (%s) error", subscriber_addr_.c_str());
        goto error_1;            
    }    
    zsock_set_sndhwm(subscriber_socket_, STSW_SUBSCRIBE_SOCKET_HWM);  
    zsock_set_linger(subscriber_socket_, 0); //no linger      
    
    
    // 
    // subscribe the channel key
    //
    std::set<std::string> subsribe_keys;
    
    // find all keys
    ReceiverSubHanderMap::iterator it;
    for(it = subsriber_handler_map_.begin();
        it != subsriber_handler_map_.end();
        it ++){
        subsribe_keys.insert(it->second.channel_name);
    }
    
    // register keys
    std::set<std::string>::iterator set_it;
    for(set_it = subsribe_keys.begin();
        set_it != subsribe_keys.end();
        set_it ++){
        zsocket_set_subscribe(subscriber_socket_, set_it->c_str());
    }    
    
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
        ret = ERROR_CODE_SYSTEM;
        goto error_2;
    }
    
    return 0;

error_2:
    

    if(subscriber_socket_ != NULL){
        zsock_destroy((zsock_t **)&subscriber_socket_);
        subscriber_socket_ = NULL;
        
    }

error_1:
    
    return ret;
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
    
    //close the subscribe socket
    //If the client start the receiver again, the new subscriber socket
    //would be created.
    if(subscriber_socket_ != NULL){
        zsock_destroy((zsock_t **)&subscriber_socket_);
        subscriber_socket_ = NULL;
        
    }       
    
    
    pthread_mutex_unlock(&lock_);  
    
}







void StreamReceiver::RegisterSubHandler(int op_code, const char * channel_name, 
                                    ReceiverSubHandler handler, void * user_data)
{

    LockGuard guard(&lock_);   

    if(IsStarted()){
        return;
    }
    
    subsriber_handler_map_[op_code].channel_name = channel_name;
    subsriber_handler_map_[op_code].handler = handler;
    subsriber_handler_map_[op_code].user_data = user_data; 


}
void StreamReceiver::UnregisterSubHandler(int op_code)
{
 
    ReceiverSubHanderMap::iterator it;
    LockGuard guard(&lock_);   

    if(IsStarted()){
        return;
    }  
    
    it = subsriber_handler_map_.find(op_code);
    if(it != subsriber_handler_map_.end()){
        subsriber_handler_map_.erase(it);
    }    
}
void StreamReceiver::UnregisterAllSubHandler()
{
    
    LockGuard guard(&lock_);   
    if(IsStarted()){
        return;
    }  
    
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

void * StreamReceiver::ThreadRoutine(void * arg)
{
    StreamReceiver * receiver = (StreamReceiver * )arg;
    zpoller_t  * poller =zpoller_new (receiver->subscriber_socket_);
    int64_t next_heartbeat_time = zclock_mono() + 
        STSW_STREAM_RECEIVER_HEARTBEAT_INT;
    
#define MAX_POLLER_WAIT_TIME    100
    
    while(receiver->flags_ & STREAM_RECEIVER_FLAG_STARTED){
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
            receiver->SubscriberHandler();
        }
             
        
        // check for heartbeat
        now = zclock_mono();
        if(now >= next_heartbeat_time){            
            receiver->Heartbeat(now);
            
            //calculate next heartbeat time
            do{
                next_heartbeat_time += STSW_STREAM_RECEIVER_HEARTBEAT_INT;
            }while(next_heartbeat_time <= now);            
        } 
        
    }
    
    if(poller != NULL){
        zpoller_destroy (&poller);
    }
        
    return NULL;
}
    


int StreamReceiver::SubscriberHandler()
{
    zframe_t * in_frame = NULL;
    char *channel_name = NULL;
    int ret;
    ret = zsock_recv(subscriber_addr_, "sf", &channel_name, &in_frame);
    if(ret || in_frame == NULL || channel_name == NULL){

        goto out;  // no frame receive
    }
    std::string in_data((const char *)zframe_data(in_frame), zframe_size(in_frame));
    
    ProtoCommonPacket msg;
    
        
    if(msg.ParseFromString(in_data)){
        

        int op_code = msg.header().code();
        ReceiverSubHanderMap::iterator it;
        pthread_mutex_lock(&lock_); 
        it = subsriber_handler_map_.find(op_code);
        if(it == subsriber_handler_map_.end()){
            pthread_mutex_unlock(&lock_); 
            
        }else{
            ReceiverSubHandlerEntry entry = it->second;
            pthread_mutex_unlock(&lock_); 
            int ret = 0;
            if(entry.channel_name == std::string(channel_name)){
                entry.handler(this, &msg, entry.user_data);
            }
       
        }//if(it == subsriber_handler_map_.end()){
    }//if(msg.ParseFromString(in_data)){


out:
    if(in_frame != NULL) {
        zframe_destroy(&in_frame);
        in_frame = NULL;
    }
    if(channel_name != NULL){
        free(channel_name);
        channel_name = NULL;
    } 
           
    return 0; 



      
}



int StreamReceiver::Heartbeat(int64_t now)
{
    if(now == 0){
        now = zclock_mono();
    }

    last_heartbeat_time_ = now;    

    ClientHeartbeatHandler(now);

}



void StreamReceiver::ClientHeartbeatHandler(int64_t now)
{
    
    LockGuard guard(&lock_);      
 #define STSW_RECEIVER_CLIENT_HEARTBEAT_TIMEOUT 5000  //wait for hearbeat reply for 5 sec
    
    if(last_send_client_heartbeat_msec_ != 0){
        //already send a client heartbeat request, but not receive a reply
        //the socket is at sending state
        
        //check if a reply is ready
        zpoller_t  * poller =zpoller_new (client_hearbeat_socket_); 
        void * socket =  zpoller_wait(poller, 0);  
        zpoller_destroy(&poller);
        if(socket != NULL){
            //a reply is ready
            zframe_t * in_frame = NULL;
            in_frame = zframe_recv(client_hearbeat_socket_);
            if(in_frame == NULL){
                return 0;  // no frame receive
            }
            std::string in_data((const char *)zframe_data(in_frame), zframe_size(in_frame));
            //after used, need free the frame
            zframe_destroy(&in_frame);  
            ProtoCommonPacket reply;
            
            if(reply.ParseFromString(in_data)){
        

                if(reply.header().code() == PROTO_PACKET_CODE_CLIENT_HEARTBEAT &&
                   reply.header().status() == PROTO_PACKET_STATUS_OK &&
                   reply.header().type() == PROTO_PACKET_TYPE_REPLY){
                       
                }else{
                    //not a valid client heartbeat reply
                    last_send_client_heartbeat_msec_ = 0;
                    next_send_client_heartbeat_msec_ = now + 5000; //resend after 5 sec                    
                }
        
        
            }else{
                //reply parse error, I have no idea, just 
                //send client heartbeat request again after a time
                last_send_client_heartbeat_msec_ = 0;
                next_send_client_heartbeat_msec_ = now + 5000; //resend after 5 sec
      
            }            

              
            

            last_send_client_heartbeat_msec_ = 0; // clean;
            
        }else{
            // no reply
            if(now - last_send_client_heartbeat_msec_ >= 
               STSW_RECEIVER_CLIENT_HEARTBEAT_TIMEOUT){
                //client heartbeat reply timeout
                //reset the socket and sending state which result into resend

                zsock_destroy((zsock_t **)&client_hearbeat_socket_);
                client_hearbeat_socket_ = NULL;
                last_send_client_heartbeat_msec_ = 0; //clean     
                next_send_client_heartbeat_msec_ = 0;
            }            
            
        }
                   
    }
    
    if(last_send_client_heartbeat_msec_ == 0 &&
       now >= next_send_client_heartbeat_msec_){
        //time to send the client heartbeat request
        
        if(client_hearbeat_socket_ == NULL){ //if socket does not exist, create it first
            client_hearbeat_socket_ = zsock_new_req(api_addr_.c_str());
            if(client_hearbeat_socket_ == NULL){
                return; // create socket failed, don't send request
            }
            zsock_set_linger(client_hearbeat_socket_, 0); //no linger              
        }
        
        // send the client heartbeat request
        
        
        
        last_send_client_heartbeat_msec_ = now;
        next_send_client_heartbeat_msec_ = 0;
    }
       
}



}





