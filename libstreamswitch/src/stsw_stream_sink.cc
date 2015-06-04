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
 * stsw_stream_receiver.cc
 *      StreamSink class implementation file, define all methods of StreamSink.
 * 
 * author: jamken
 * date: 2014-12-5
**/ 

#include <stsw_stream_sink.h>
#include <stdint.h>
#include <list>
#include <set>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <czmq.h>

#include <stsw_lock_guard.h>

#include <stsw_sink_listener.h>

#include <pb_packet.pb.h>
#include <pb_client_heartbeat.pb.h>
#include <pb_media.pb.h>
#include <pb_stream_info.pb.h>
#include <pb_metadata.pb.h>
#include <pb_media_statistic.pb.h>
#include <pb_client_list.pb.h>


namespace stream_switch {

    
    
 static int ReplyStatus2ErrorCode(const ProtoCommonPacket &reply,  std::string *err_info){
    
    int ret;
    //check reply status

    if(reply.header().status() == PROTO_PACKET_STATUS_OK){
        ret = ERROR_CODE_OK;
            
    }else if(reply.header().status()>= 400 && reply.header().status() <= 499){
        ret = ERROR_CODE_CLIENT;
    }else if(reply.header().status()>= 500 && reply.header().status() <= 599){
        ret = ERROR_CODE_SERVER;
    }else{
        ret = ERROR_CODE_GENERAL;            
    }
    
    if(ret){
        char temp[64];
        sprintf(temp, " (status:%d)", reply.header().status());
        if(err_info){
            err_info->assign(reply.header().info());
            err_info->append(temp);
        }
    }
    
    return ret; 
    
}        


////////////////////////////////////////////////////////////////////////
//Implementation of RpcResult
class MsgRpcResult: public RpcResult
{
    
public:
    virtual ProtoCommonPacket * GetReply()
    {
        return &reply_;
    }
    virtual const char * ExtraBlob()
    {
        if(blob_frame_ != NULL){
            return (const char *)zframe_data(blob_frame_);
        }else{
            return NULL;
        }
    }
    virtual size_t BlobSize()
    {
        if(blob_frame_ != NULL){
            return zframe_size(blob_frame_);
        }else{
            return 0;
        }        
    }
    
    MsgRpcResult():blob_frame_(NULL)
    {
        
    }

    virtual ~MsgRpcResult()
    {
        if(blob_frame_ != NULL){
            zframe_destroy(&blob_frame_);
            blob_frame_ = NULL;
        }
    }
    
    virtual void set_blob_frame(zframe_t * blob_frame)
    {
        blob_frame_ = blob_frame;
    }
    
    virtual zframe_t * blob_frame()
    {
        return blob_frame_;
    }
    
    virtual ProtoCommonPacket * mutable_reply()
    {
        return &reply_;
    }
private:
    
    ProtoCommonPacket reply_;
    
    zframe_t * blob_frame_;
    
};





    
StreamSink::StreamSink()
:last_api_socket_(NULL), client_hearbeat_socket_(NULL), subscriber_socket_(NULL),
worker_thread_id_(0), next_seq_(1), debug_flags_(0), flags_(0), 
last_heartbeat_time_(0), 
last_send_client_heartbeat_msec_(0),
next_send_client_heartbeat_msec_(0), 
listener_(NULL), sub_queue_size_(STSW_SUBSCRIBE_SOCKET_HWM), 
last_frame_ssrc_(0)
{
    
}


StreamSink::~StreamSink()
{
    Uninit();
}

int StreamSink::InitRemote(const std::string &source_ip, int source_tcp_port, 
                               const StreamClientInfo &client_info, 
                               uint32_t sub_queue_size, 
                               SinkListener *listener,
                               uint32_t debug_flags,
                               std::string *err_info)
{
    int ret;
    
    if(source_ip.size() == 0 || source_tcp_port == 0){
        SET_ERR_INFO(err_info, "ip or port is invalid");
        return ERROR_CODE_PARAM;
    }
    
    //generate api socket address

#define MAX_SOCKET_BIND_ADDR_LEN 255    

    char tmp_addr[MAX_SOCKET_BIND_ADDR_LEN + 1];
    memset(tmp_addr, 0, MAX_SOCKET_BIND_ADDR_LEN + 1);

    ret = snprintf(tmp_addr, MAX_SOCKET_BIND_ADDR_LEN, 
                   ">tcp://%s:%d",  source_ip.c_str(), source_tcp_port);

    if(ret == MAX_SOCKET_BIND_ADDR_LEN){
        ret = ERROR_CODE_PARAM;
        fprintf(stderr, "socket address too long: %s\n", tmp_addr);
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
        fprintf(stderr, "socket address too long: %s\n", tmp_addr);
        SET_ERR_INFO(err_info, "socket address too long");
        goto error_out;            
    }else if(ret < 0){
        ret = ERROR_CODE_SYSTEM;
        perror("snprintf for socket address failed");
        SET_ERR_INFO(err_info, "snprintf for socket address failed");
        goto error_out;              
    }
    subscriber_addr_ = tmp_addr;
    
    ret = InitBase(client_info, sub_queue_size, listener, 
                   debug_flags, err_info);
    if(ret){
        goto error_out;
    }
    
    return 0;

error_out:
    api_addr_.clear();
    subscriber_addr_.clear();
    
    return ret;
    
    
}
int StreamSink::InitLocal(const std::string &stream_name, 
                              const StreamClientInfo &client_info, 
                              uint32_t sub_queue_size, 
                              SinkListener *listener,
                              uint32_t debug_flags,                              
                              std::string *err_info)
{
    int ret;

    if(stream_name.size() == 0){
        SET_ERR_INFO(err_info, "stream_name cannot be empty");
        return ERROR_CODE_PARAM;
    }
    
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
        fprintf(stderr, "socket address too long: %s\n", tmp_addr);
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
        fprintf(stderr, "socket address too long: %s\n", tmp_addr);
        SET_ERR_INFO(err_info, "socket address too long");
        goto error_out;            
    }else if(ret < 0){
        ret = ERROR_CODE_SYSTEM;
        perror("snprintf for socket address failed");
        SET_ERR_INFO(err_info, "snprintf for socket address failed");
        goto error_out;              
    }
    subscriber_addr_ = tmp_addr;
    
    ret = InitBase(client_info, sub_queue_size, 
                   listener, debug_flags, err_info);
    if(ret){
        goto error_out;
    }
    
    return 0;

error_out:
    api_addr_.clear();
    subscriber_addr_.clear();
    
    return ret;


}


int StreamSink::InitBase(const StreamClientInfo &client_info, 
                             uint32_t sub_queue_size, 
                             SinkListener *listener,
                             uint32_t debug_flags, 
                             std::string *err_info)
{
    int ret;
    
    if((flags_ & STREAM_RECEIVER_FLAG_INIT) != 0){
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
        fprintf(stderr, "zsock_new_req create api socket failed: maybe address (%s) error\n", api_addr_.c_str());
        goto error_1;            
    }
    zsock_set_linger(client_hearbeat_socket_, 0); //no linger   
    

    
    //init handlers
    RegisterSubHandler(PROTO_PACKET_CODE_MEDIA, 
                       STSW_PUBLISH_MEDIA_CHANNEL, 
                       (SinkSubHandler)StaticMediaFrameHandler, this);
 


    //init client info
    client_info_ = client_info;
    
    debug_flags_ = debug_flags;
    
    set_listener(listener);   
    
    sub_queue_size_ = sub_queue_size;
    
    flags_ |= STREAM_RECEIVER_FLAG_INIT;        
   
    return 0;
    
//error_2:

    
    if(client_hearbeat_socket_ != NULL){
        zsock_destroy((zsock_t **)&client_hearbeat_socket_);
        client_hearbeat_socket_ = NULL;
        
    }
error_1:    
    
    pthread_mutex_destroy(&lock_);  
    
    
error_0:
    
    return ret;
    
} 




void StreamSink::Uninit()
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


bool StreamSink::IsInit()
{
    return (flags_ & STREAM_RECEIVER_FLAG_INIT) != 0;
}


bool StreamSink::IsStarted()
{
     return (flags_ & STREAM_RECEIVER_FLAG_STARTED) != 0;   
}

StreamMetadata StreamSink::stream_meta()
{
    LockGuard guard(&lock_);  
    return stream_meta_;
}

StreamClientInfo StreamSink::client_info()
{
    LockGuard guard(&lock_);  
    return client_info_;    
}

void StreamSink::set_client_info(const StreamClientInfo &client_info)
{
    LockGuard guard(&lock_); 
    client_info_ = client_info;
}



int StreamSink::Start(std::string *err_info)
{
    int ret;
    std::set<std::string> subsribe_keys;
    ReceiverSubHanderMap::iterator it;
    std::set<std::string>::iterator set_it;
    
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

    
    //
    // create a new subscriber socket
    // moving subscribe socket from init() to start() is for cleanning 
    // the socket context to avoid receiving a overdue broadcast 
    // frame. If create subscrber socket in init() ,and keep the same 
    // subscribe socket for the whole life time of receiver, the receiver
    // may get a overdue frame ater next time start()
    subscriber_socket_ = zsock_new(ZMQ_SUB);
    if(subscriber_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new create subsriber socket failed");          
        fprintf(stderr, "zsock_new create subsriber socket failed\n");
        goto error_1;            
    }    
    zsock_set_sndhwm(subscriber_socket_, sub_queue_size_);  
    zsock_set_rcvhwm(subscriber_socket_, sub_queue_size_);      
    zsock_set_linger(subscriber_socket_, 0); //no linger      
    if(zsock_attach((zsock_t *)subscriber_socket_, subscriber_addr_.c_str(), false)){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_attach() for the subsriber socket failed: maybe address error");          
        fprintf(stderr, "zsock_attach() for the subsriber socket failed: "
                "maybe address (%s) error\n", subscriber_addr_.c_str());
        goto error_2;          
    }
    
    // 
    // subscribe the channel key
    //
        
    // find all keys    
    for(it = subsriber_handler_map_.begin();
        it != subsriber_handler_map_.end();
        it ++){
        subsribe_keys.insert(it->second.channel_name);
    }
    
    // register keys    
    for(set_it = subsribe_keys.begin();
        set_it != subsribe_keys.end();
        set_it ++){
        zsock_set_subscribe(subscriber_socket_, set_it->c_str());
    }    

    last_send_client_heartbeat_msec_ = 0;
    next_send_client_heartbeat_msec_ = 0;
    last_frame_ssrc_ = stream_meta_.ssrc;
    
    //start the internal thread
    ret = pthread_create(&worker_thread_id_, NULL, StreamSink::StaticThreadRoutine, this);
    if(ret){
        if(err_info){
            *err_info = "pthread_create failed:";
            //char tmp[64];
            //*err_info += strerror_r(errno, tmp, 64);
            *err_info += strerror(errno);
        }

        perror("Start Source internal thread failed");
        worker_thread_id_  = 0;
        ret = ERROR_CODE_SYSTEM;
        goto error_2;
    }

    flags_ |= STREAM_RECEIVER_FLAG_STARTED;    
    
    return 0;

error_2:


    if(subscriber_socket_ != NULL){
        zsock_destroy((zsock_t **)&subscriber_socket_);
        subscriber_socket_ = NULL;
        
    }

error_1:
    
    return ret;
}


void StreamSink::Stop()
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

void StreamSink::RegisterSubHandler(int op_code, const char * channel_name, 
                                    SinkSubHandler handler, void * user_data)
{

    LockGuard guard(&lock_);   

    if(IsStarted()){
        return;
    }
    
    subsriber_handler_map_[op_code].channel_name = channel_name;
    subsriber_handler_map_[op_code].handler = handler;
    subsriber_handler_map_[op_code].user_data = user_data; 


}
void StreamSink::UnregisterSubHandler(int op_code)
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
void StreamSink::UnregisterAllSubHandler()
{
    
    LockGuard guard(&lock_);   
    if(IsStarted()){
        return;
    }  
    
    subsriber_handler_map_.clear();    
}      

int StreamSink::StaticMediaFrameHandler(void * user_data, const ProtoCommonPacket &msg, 
                                        const char * extra_blob, size_t blob_size)
{
    StreamSink *receiver = (StreamSink *)user_data;
    return receiver->MediaFrameHandler(msg, extra_blob, blob_size);
}
int StreamSink::MediaFrameHandler(const ProtoCommonPacket &msg, 
                                  const char * extra_blob, size_t blob_size)
{
    MediaFrameInfo frame_info;
    const char *frame_data;
    size_t frame_size;
    int ret;
    uint64_t seq;
    
    //extract media frame from message
    do{
        ProtoMediaFrameMsg frame_msg;
        if(! frame_msg.ParseFromString(msg.body())){
            //body parse error
            ret = ERROR_CODE_PARSE;
            fprintf(stderr, "media frame Parse Error\n");
            return ret;                
        } 
        
        if(debug_flags() & DEBUG_FLAG_DUMP_PUBLISH){
            fprintf(stderr, "Decode the following body from a PROTO_PACKET_CODE_MEDIA message in MediaFrameHandler:\n");
            fprintf(stderr, "%s\n", frame_msg.DebugString().c_str());
        }    
        


        frame_info.sub_stream_index = frame_msg.stream_index();
        frame_info.frame_type = (MediaFrameType)frame_msg.frame_type();
        frame_info.ssrc = frame_msg.ssrc();
        frame_info.timestamp.tv_sec = frame_msg.sec();
        frame_info.timestamp.tv_usec = frame_msg.usec();
        
        //for ProtoMediaFrameMsg packet, attached blob is the frame data
        frame_data = extra_blob;
        frame_size = blob_size;
        
        seq = frame_msg.seq();
        
    }while(0); //free frame_msg at once

    //
    // update statistic_
 
    pthread_mutex_lock(&lock());
    
    // check ssrc
    if(stream_meta_.ssrc != frame_info.ssrc){
        //ssrc mismatch, just ignore this frame
        if(frame_info.ssrc != last_frame_ssrc_){ //Jamken: avoid repeatly callback the same error
            last_frame_ssrc_ = frame_info.ssrc;
            SinkListener *plistener = listener();
    
            pthread_mutex_unlock(&lock());   
 
            if(plistener != NULL){
                plistener->OnMetadataMismatch(frame_info.ssrc);
            }
        }else{
            //Jamken: this error has already callback.
            pthread_mutex_unlock(&lock()); 
        }
        
        
        return 0;
    }
        
    if(frame_info.sub_stream_index < 0 ||
        frame_info.sub_stream_index >= (int32_t)stream_meta_.sub_streams.size()){
        //sub stream index mismatch, just ignore this frame
        pthread_mutex_unlock(&lock());
        return 0;
    }        
    
    int sub_stream_index = frame_info.sub_stream_index;
    
    if(frame_info.frame_type == MEDIA_FRAME_TYPE_KEY_FRAME ||
        frame_info.frame_type == MEDIA_FRAME_TYPE_DATA_FRAME){
        //the frames contains media data   
        statistic_[sub_stream_index].data_frames++;
        statistic_[sub_stream_index].data_bytes += frame_size;
        if( (statistic_[sub_stream_index].last_seq != 0) 
            && (seq > (statistic_[sub_stream_index].last_seq + 1))){
            statistic_[sub_stream_index].lost_frames +=  
                (seq - (statistic_[sub_stream_index].last_seq + 1));
        }
        statistic_[sub_stream_index].last_seq = seq;

     
        if(frame_info.frame_type == MEDIA_FRAME_TYPE_KEY_FRAME){
            statistic_[sub_stream_index].key_frames ++;
            statistic_[sub_stream_index].key_bytes += frame_size;
                    
                //start a new gov
            statistic_[sub_stream_index].last_gov = 
                statistic_[sub_stream_index].cur_gov;
            statistic_[sub_stream_index].cur_gov = 0;            
        }
        statistic_[sub_stream_index].cur_gov ++;
            
            
    }else{ 
            //not contain media data, so that not update last_seq

    }//if(frame_type == MEDIA_FRAME_TYPE_KEY_FRAME ||          
            
    

    SinkListener *plistener = listener();
    
    pthread_mutex_unlock(&lock());   
 
    if(plistener != NULL){
        plistener->OnLiveMediaFrame(frame_info, frame_data, frame_size);
    }    
    
       
    return 0;
}

void * StreamSink::StaticThreadRoutine(void *arg)
{
    StreamSink * receiver = (StreamSink * )arg;
    receiver->InternalRoutine();
    return NULL;
}


void StreamSink::InternalRoutine()
{
    zpoller_t  * poller =zpoller_new (subscriber_socket_, NULL);
    int64_t next_heartbeat_time = zclock_mono() + 
        STSW_STREAM_RECEIVER_HEARTBEAT_INT;
    
#define MAX_POLLER_WAIT_TIME    100
    
    while(flags_ & STREAM_RECEIVER_FLAG_STARTED){
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
            OnSubRead();
        }
             
        
        // check for heartbeat
        now = zclock_mono();
        if(now >= next_heartbeat_time){            
            Heartbeat(now);
            
            //calculate next heartbeat time
            do{
                next_heartbeat_time += STSW_STREAM_RECEIVER_HEARTBEAT_INT;
            }while(next_heartbeat_time <= now);            
        } 
        
    }
    
    if(poller != NULL){
        zpoller_destroy (&poller);
    }

}
    


void StreamSink::OnSubRead()
{
    zframe_t * packet_frame = NULL, * blob_frame = NULL;
    char *channel_name = NULL;
    ProtoCommonPacket msg;
    const char * extra_blob = NULL;
    size_t blob_size = 0;

    zmsg_t *zmsg = zmsg_recv (subscriber_socket_);
    if (!zmsg){
        goto out; //  Interrupted
    }
    channel_name = zmsg_popstr(zmsg);
    if(channel_name == NULL){
        goto out;  // invalid;
    }
    packet_frame = zmsg_pop(zmsg);
    if(packet_frame == NULL){
        goto out;  // invalid
    }
    blob_frame = zmsg_pop(zmsg);
    if(blob_frame != NULL){
        extra_blob = (const char *)zframe_data(blob_frame);
        blob_size = zframe_size(blob_frame);
        if(blob_size == 0 && extra_blob != NULL){ //check
            extra_blob = NULL;
        }
    }
    //release the zmsg as early as possible
    if(zmsg != NULL){
        zmsg_destroy (&zmsg);
        zmsg = NULL;        
    }

    if(msg.ParseFromArray((const void *)zframe_data(packet_frame), 
                          (int)zframe_size(packet_frame))){
        
        if(debug_flags() & DEBUG_FLAG_DUMP_PUBLISH){
            fprintf(stderr, "Received the following packet (with blob size:%d) from subsriber socket channel %s (timestamp:%lld ms):\n", 
                    (int)blob_size, 
                    channel_name, 
                    (long long)zclock_time());
            fprintf(stderr, "%s\n", msg.DebugString().c_str());
        }
                
        OnSubMsg(channel_name, msg, extra_blob, blob_size);

    }//if(msg.ParseFromString(in_data)){


out:

    if(blob_frame != NULL) {
        zframe_destroy(&blob_frame);
        blob_frame = NULL;
    }
    if(packet_frame != NULL) {
        zframe_destroy(&packet_frame);
        packet_frame = NULL;
    }
    if(channel_name != NULL){
        free(channel_name);
        channel_name = NULL;
    } 
    if(zmsg != NULL){
        zmsg_destroy (&zmsg);
        zmsg = NULL;        
    }
           
    return; 
      
}


void StreamSink::OnSubMsg(std::string channel_name, const ProtoCommonPacket &msg, 
                          const char * extra_blob, size_t blob_size)
{
    int op_code = msg.header().code();
    ReceiverSubHanderMap::iterator it;
    pthread_mutex_lock(&lock_); 
    it = subsriber_handler_map_.find(op_code);
    if(it == subsriber_handler_map_.end()){
        pthread_mutex_unlock(&lock_); 
            
    }else{
        SinkSubHandlerEntry entry = it->second;
        pthread_mutex_unlock(&lock_); 
        if(entry.channel_name == channel_name){
            entry.handler(entry.user_data, msg, extra_blob, blob_size);
        }
       
    }//if(it == subsriber_handler_map_.end())    
    
}


int StreamSink::Heartbeat(int64_t now)
{
    if(now == 0){
        now = zclock_mono();
    }

    last_heartbeat_time_ = now;    

    ClientHeartbeatHandler(now);
    
    return 0;

}



void StreamSink::ClientHeartbeatHandler(int64_t now)
{
    
    LockGuard guard(&lock_);      
 #define STSW_RECEIVER_CLIENT_HEARTBEAT_TIMEOUT 3000  //3 sec timeout for hearbeat reply
    
    if(last_send_client_heartbeat_msec_ != 0){
        //already send a client heartbeat request, but not receive a reply
        //the socket is at sending state
        
        //check if a reply is ready
        zpoller_t  * poller =zpoller_new (client_hearbeat_socket_, NULL); 
        void * socket =  zpoller_wait(poller, 0);  
        zpoller_destroy(&poller);
        if(socket != NULL){
            //a reply is ready
            zframe_t * in_frame = NULL;
            in_frame = zframe_recv(client_hearbeat_socket_);
            if(in_frame == NULL){
                return;// no frame receive
            }

            ProtoCommonPacket reply;
            
            if(reply.ParseFromArray((const char *)zframe_data(in_frame), zframe_size(in_frame))){
                
                if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
                    fprintf(stderr, 
                            "Received the following packet from client heartbeat socket (timestamp:%lld ms):\n",
                             (long long)zclock_time());
                    fprintf(stderr, "%s\n", reply.DebugString().c_str());
                }               
        

                if(reply.header().code() == PROTO_PACKET_CODE_CLIENT_HEARTBEAT &&
                   reply.header().status() == PROTO_PACKET_STATUS_OK &&
                   reply.header().type() == PROTO_PACKET_TYPE_REPLY){
                    std::string body = reply.body();
                    ProtoClientHeartbeatRep client_heartbeat_reply;
                    if(client_heartbeat_reply.ParseFromString(body)){

                        if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
                            fprintf(stderr, 
                                    "Decode the following body from a PROTO_PACKET_CODE_CLIENT_HEARTBEAT reply:\n");
                            fprintf(stderr, "%s\n", client_heartbeat_reply.DebugString().c_str());
                        } 
                        
                        int lease = client_heartbeat_reply.lease();
                        // send next heartbeat request at 1/3 lease
                        next_send_client_heartbeat_msec_ = 
                            last_send_client_heartbeat_msec_ + lease * 1000 / 3;                        
                        
                    }else{
                        //not a valid client heartbeat reply
                        next_send_client_heartbeat_msec_ = now + 5000; //resend after 5 sec                               
                    }                       
                }else{
                    //not a valid client heartbeat reply
                    next_send_client_heartbeat_msec_ = now + 5000; //resend after 5 sec                    
                }
       
            }else{
                //reply parse error, I have no idea, just 
                //send client heartbeat request again after a time
                 next_send_client_heartbeat_msec_ = now + 5000; //resend after 5 sec
            }               
            
            last_send_client_heartbeat_msec_ = 0; // clean;
            zframe_destroy(&in_frame);
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
        ProtoClientHeartbeatReq client_heartbeat_req;
        ProtoCommonPacket request;
        client_heartbeat_req.set_client_ip_version(
            (ProtoClientIPVersion)client_info_.client_ip_version);
        client_heartbeat_req.set_client_ip(client_info_.client_ip);
        client_heartbeat_req.set_client_protocol(client_info_.client_protocol);
        client_heartbeat_req.set_client_port(client_info_.client_port);
        client_heartbeat_req.set_client_text(client_info_.client_text);
        client_heartbeat_req.set_client_token(client_info_.client_token);


        if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
            fprintf(stderr, 
                    "Encode the following body into a PROTO_PACKET_CODE_CLIENT_HEARTBEAT request:\n");
            fprintf(stderr, "%s\n", client_heartbeat_req.DebugString().c_str());
        } 

        request.mutable_header()->set_type(PROTO_PACKET_TYPE_REQUEST);
        request.mutable_header()->set_seq(next_seq_++);
        request.mutable_header()->set_code(PROTO_PACKET_CODE_CLIENT_HEARTBEAT);
        client_heartbeat_req.SerializeToString(request.mutable_body());
        if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
            fprintf(stderr, 
                    "Send out the following packet to client heartbeat socket (timestamp:%lld ms):\n",
                    (long long)zclock_time());
            fprintf(stderr, "%s\n", request.DebugString().c_str());
        }           
        
        
        std::string out_data;
        request.SerializeToString(&out_data);
        zframe_t * out_frame = NULL;
        out_frame = zframe_new(out_data.data(), out_data.size());
        zframe_send(&out_frame, client_hearbeat_socket_, ZFRAME_DONTWAIT);         
        
        last_send_client_heartbeat_msec_ = now;
        next_send_client_heartbeat_msec_ = 0;
    }
       
}


int StreamSink::UpdateStreamMetaData(int timeout, StreamMetadata * metadata, std::string *err_info)
{
    ProtoCommonPacket request;
    ProtoCommonPacket *reply = NULL;
    RpcResult * result = NULL;
    int ret;
    
    if(IsStarted()){
        ret = ERROR_CODE_GENERAL;
        SET_ERR_INFO(err_info, "Cannot Setup MetaData After Start");
        return ret;          
    }
    

    request.mutable_header()->set_type(PROTO_PACKET_TYPE_REQUEST);
    request.mutable_header()->set_seq(GetNextSeq());
    request.mutable_header()->set_code(PROTO_PACKET_CODE_METADATA);    


    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode no body into a PROTO_PACKET_CODE_METADATA request\n");
    }

    ret = SendRpcRequest(&request, NULL, 0, timeout, &result, err_info);
    if(ret){
        //error
        return ret;
    }
    reply = result->GetReply();
    
    ret = ReplyStatus2ErrorCode(*reply, err_info);
    if(ret){
        SAFE_DELETE(result);
        return ret;
    }  
    
    //extract metadata from reply
    ProtoMetaRep metadata_rep;
    if(! metadata_rep.ParseFromString(reply->body())){
        //body parse error
        ret = ERROR_CODE_PARSE;
        SET_ERR_INFO(err_info, "reply body parse to metadata error");
        SAFE_DELETE(result);
        return ret;                
    }
    
    //no need the result    
    SAFE_DELETE(result);
    
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode the following body from a PROTO_PACKET_CODE_METADATA reply:\n");
        fprintf(stderr, "%s\n", metadata_rep.DebugString().c_str());
    }    
    
    
    //configure the metadata in receiver
    {
        LockGuard guard(&lock_);  
        int sub_stream_num = metadata_rep.sub_streams_size();
        
        
        //update the metadata in the sink
        
        stream_meta_.play_type = (StreamPlayType) metadata_rep.play_type();
        stream_meta_.source_proto = metadata_rep.source_proto();
        stream_meta_.ssrc = metadata_rep.ssrc();
        stream_meta_.bps = metadata_rep.bps();  
        stream_meta_.sub_streams.reserve(sub_stream_num);

        ::google::protobuf::RepeatedPtrField< ::stream_switch::ProtoSubStreamInfo >::const_iterator it;    
        for(it = metadata_rep.sub_streams().begin();
            it != metadata_rep.sub_streams().end();
            it ++){
            
            SubStreamMetadata sub_stream;
            sub_stream.media_type = (SubStreamMediaType)it->media_type();
            sub_stream.codec_name = it->codec_name();      
            sub_stream.direction = (SubStreamDirectionType)it->direction();    
            sub_stream.extra_data = it->extra_data();  

            switch(sub_stream.media_type){
                case SUB_STREAM_MEIDA_TYPE_VIDEO:
                {
                    sub_stream.media_param.video.height = it->height();
                    sub_stream.media_param.video.width = it->width();
                    sub_stream.media_param.video.fps = it->fps();                
                    sub_stream.media_param.video.gov = it->gov();
                    
                    break;
                }
                case SUB_STREAM_MEIDA_TYPE_AUDIO:
                {
                    sub_stream.media_param.audio.samples_per_second = it->samples_per_second();
                    sub_stream.media_param.audio.channels = it->channels();
                    sub_stream.media_param.audio.bits_per_sample = it->bits_per_sample();                
                    sub_stream.media_param.audio.sampele_per_frame = it->sampele_per_frame();                
                    
                    break;
                }
                case SUB_STREAM_MEIDA_TYPE_TEXT:
                {
                    sub_stream.media_param.text.x = it->x();
                    sub_stream.media_param.text.y = it->y();
                    sub_stream.media_param.text.fone_size = it->fone_size();                
                    sub_stream.media_param.text.font_type = it->font_type();                   
                    
                    break;
                }
                default:
                {
                    break;
                    
                }
            }
            stream_meta_.sub_streams.push_back(sub_stream); 
        }
        
        // clear the statistic
        statistic_.clear();
        statistic_.resize(sub_stream_num);
        int i;
        for(i=0;i<sub_stream_num;i++){
            statistic_[i].sub_stream_index = i;
            statistic_[i].media_type = (SubStreamMediaType)stream_meta_.sub_streams[i].media_type;     
        }  
        
        if(metadata != NULL){
            *metadata = stream_meta_;
        }
      
    }
    
    return 0;
}

int StreamSink::SourceStatistic(int timeout, MediaStatisticInfo * statistic, std::string *err_info)
{
    ProtoCommonPacket request;
    ProtoCommonPacket *reply = NULL;
    RpcResult * result = NULL;
    int ret;
    
    if(statistic == NULL){
        ret = ERROR_CODE_PARAM;
        SET_ERR_INFO(err_info, "statistic cannot be NULL");
        return ret;          
    }
    
    request.mutable_header()->set_type(PROTO_PACKET_TYPE_REQUEST);
    request.mutable_header()->set_seq(GetNextSeq());
    request.mutable_header()->set_code(PROTO_PACKET_CODE_MEDIA_STATISTIC);    

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode no body into a PROTO_PACKET_CODE_MEDIA_STATISTIC request\n");
    }

    ret = SendRpcRequest(&request, NULL, 0, timeout, &result, err_info);
    if(ret){
        //error
        return ret;
    }
    
    reply = result->GetReply();

    
    
    ret = ReplyStatus2ErrorCode(*reply, err_info);
    if(ret){
        SAFE_DELETE(result);
        return ret;
    }  
    
    //extract statistic from reply
    ProtoMediaStatisticRep statistic_rep;
    if(! statistic_rep.ParseFromString(reply->body())){
        //body parse error
        ret = ERROR_CODE_PARSE;
        SET_ERR_INFO(err_info, "reply body parse to statistic error");
        SAFE_DELETE(result);
        return ret;                
    }
    
    SAFE_DELETE(result);
    
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode the following body from a PROTO_PACKET_CODE_MEDIA_STATISTIC reply\n");
        fprintf(stderr, "%s\n", statistic_rep.DebugString().c_str());
    }       
    
    statistic->ssrc = statistic_rep.ssrc();
    statistic->timestamp = statistic_rep.timestamp();    
    statistic->sum_bytes = statistic_rep.sum_bytes();
    statistic->sub_streams.reserve(statistic_rep.sub_stream_stats_size());

    ::google::protobuf::RepeatedPtrField< ::stream_switch::ProtoSubStreamMediaStatistic >::const_iterator it;    
    for(it = statistic_rep.sub_stream_stats().begin();
        it != statistic_rep.sub_stream_stats().end();
        it ++){
        SubStreamMediaStatistic sub_stream;  
        sub_stream.sub_stream_index = it->sub_stream_index();
        sub_stream.media_type = (SubStreamMediaType)it->media_type();        
        sub_stream.data_bytes = it->data_bytes();
        sub_stream.key_bytes = it->key_bytes();
        sub_stream.lost_frames = it->lost_frames();
        sub_stream.data_frames = it->data_frames();        
        sub_stream.key_frames = it->key_frames();
        sub_stream.last_gov = it->last_gov();
        statistic->sub_streams.push_back(sub_stream); 
    }
    
    return 0;
    
}  
  
int StreamSink::KeyFrame(int timeout, std::string *err_info)
{
    ProtoCommonPacket request;
    ProtoCommonPacket *reply = NULL;
    RpcResult * result = NULL;
    int ret;
    
    request.mutable_header()->set_type(PROTO_PACKET_TYPE_REQUEST);
    request.mutable_header()->set_seq(GetNextSeq());
    request.mutable_header()->set_code(PROTO_PACKET_CODE_KEY_FRAME);

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Send out a PROTO_PACKET_CODE_KEY_FRAME request with no body\n");
    }

    ret = SendRpcRequest(&request, NULL, 0, timeout, &result, err_info);
    if(ret){
        //error
        return ret;
    }
    reply = result->GetReply();
 
   
    ret = ReplyStatus2ErrorCode(*reply, err_info);
    if(ret){
        SAFE_DELETE(result);
        return ret;
    }
    SAFE_DELETE(result);
    
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode no body from a PROTO_PACKET_CODE_KEY_FRAME reply\n");
    }    

    return 0;        
}



int StreamSink::ClientList(int timeout, uint32_t start_index, uint32_t request_num, 
                           uint32_t *  total_num, StreamClientList * client_list, 
                           std::string *err_info)
{
    ProtoCommonPacket request;
    ProtoCommonPacket *reply = NULL;
    RpcResult * result = NULL;
    ProtoClientListReq client_list_req_body;
    int ret;
    
    
    client_list_req_body.set_client_num(request_num);
    client_list_req_body.set_start_index(start_index);
   
    request.mutable_header()->set_type(PROTO_PACKET_TYPE_REQUEST);
    request.mutable_header()->set_seq(GetNextSeq());
    request.mutable_header()->set_code(PROTO_PACKET_CODE_CLIENT_LIST);    
    client_list_req_body.SerializeToString(request.mutable_body());    

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_CLIENT_LIST request\n");
        fprintf(stderr, "%s\n", client_list_req_body.DebugString().c_str());
    }

    ret = SendRpcRequest(&request, NULL, 0, timeout, &result, err_info);
    if(ret){
        //error
        return ret;
    } 
    reply = result->GetReply();
    
    ret = ReplyStatus2ErrorCode(*reply, err_info);
    if(ret){
        SAFE_DELETE(result);
        return ret;
    }  
    
    //extract statistic from reply
    ProtoClientListRep client_list_rep;
    if(! client_list_rep.ParseFromString(reply->body())){
        //body parse error
        ret = ERROR_CODE_PARSE;
        SET_ERR_INFO(err_info, "reply body parse to client_list error");
        SAFE_DELETE(result);
        return ret;                
    }
    
    SAFE_DELETE(result);
    
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode the following body from a PROTO_PACKET_CODE_CLIENT_LIST reply\n");
        fprintf(stderr, "%s\n", client_list_rep.DebugString().c_str());
    }       
    
    if(total_num){
        *total_num = client_list_rep.total_num();
    }
    
    if(client_list){
        ::google::protobuf::RepeatedPtrField< ::stream_switch::ProtoClientHeartbeatReq >::const_iterator it;    
        for(it = client_list_rep.client_list().begin();
            it != client_list_rep.client_list().end();
            it ++){
            StreamClientInfo client_info;  
            client_info.client_ip = it->client_ip();
            client_info.client_ip_version = (StreamClientIPVersion)it->client_ip_version();
            client_info.client_port = it->client_port();
            client_info.client_protocol = it->client_protocol();
            client_info.client_text = it->client_text();
            client_info.client_token = it->client_token();
            client_info.last_active_time = it->last_active_time();
            
            client_list->push_back(client_info);
        }        
    }

    
    return 0;
    
} 


uint32_t StreamSink::GetNextSeq()
{
    uint32_t seq;
    LockGuard guard(&lock_);   
    seq = next_seq_++;
    
    return seq;    
    
}




void StreamSink::ReceiverStatistic(MediaStatisticInfo * statistic)
{
    if(statistic == NULL){
        return;
    }

    LockGuard guard(&lock_);   
    statistic->ssrc = stream_meta_.ssrc;
    statistic->timestamp = (int64_t)time(NULL);
    statistic->sub_streams = statistic_;

    SubStreamMediaStatisticVector::iterator it;
    for(it = statistic_.begin(); 
        it != statistic_.end();
        it++){

        statistic->sum_bytes += it->data_bytes;     
    }
    
}



int StreamSink::SendRpcRequest(ProtoCommonPacket * request, const char * extra_blob, size_t blob_size, 
                               int timeout, RpcResult **result,  std::string *err_info)
{
    int ret = 0;    
    
    if(request == NULL || result == NULL){
        SET_ERR_INFO(err_info, "request/result cannot be NULL");          
        return ERROR_CODE_PARAM;        
    }
    if(timeout < 0){
        SET_ERR_INFO(err_info, "timeout cannot be less than 0");          
        return ERROR_CODE_PARAM;        
    }
    if(request->header().type() != PROTO_PACKET_TYPE_REQUEST){
        SET_ERR_INFO(err_info, "Request' packet type is incorrect");          
        return ERROR_CODE_PARAM;         
    }
    
    if(!IsInit()){
        SET_ERR_INFO(err_info, "Receiver not init");          
        return ERROR_CODE_PARAM;
    }    
    
    
    SocketHandle api_socket = NULL;
    std::string api_addr;
    std::string out_data;
    
    zframe_t * in_frame = NULL, *in_blob_frame = NULL;
    zpoller_t  * poller = NULL;
    SocketHandle reader_socket = NULL;  
    zmsg_t *out_zmsg = NULL, *in_zmsg = NULL;
    MsgRpcResult * in_result = NULL;
    ProtoCommonPacket * reply = NULL;
    
    // get api socket
    pthread_mutex_lock(&lock_); 
    api_socket = last_api_socket_;
    last_api_socket_ = NULL;
    api_addr = api_addr_;        
    pthread_mutex_unlock(&lock_);  

    if(api_socket == NULL){
        //no api_socket is idle, just create a new one
        api_socket = zsock_new_req(api_addr.c_str());
        if(api_socket == NULL){
            ret = ERROR_CODE_SYSTEM;
            SET_ERR_INFO(err_info, "zsock_new_req create api socket failed: maybe address error");   
            fprintf(stderr, "zsock_new_req create api socket failed: maybe address (%s) error\n", api_addr.c_str());
            return ret;         
        }
        zsock_set_linger(api_socket, 0); //no linger                   
    }    

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Send out the following packet (with blob size: %d)  to api socket (timestamp:%lld ms):\n", 
                (int)blob_size, 
                (long long)zclock_time());
        fprintf(stderr, "%s\n", request->DebugString().c_str());
    }

    //
    // send the request
    //    
    if(request->SerializeToString(&out_data) == false){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "failed to serialize the request");   
        goto error_1;                
    };
    //out_frame = zframe_new(out_data.data(), out_data.size());
    //zframe_send(&out_frame, api_socket, ZFRAME_DONTWAIT);    

    out_zmsg = zmsg_new ();
    zmsg_addmem(out_zmsg, out_data.data(), out_data.size());
    if(extra_blob != NULL && blob_size != 0){
        zmsg_addmem(out_zmsg, extra_blob, blob_size);
    }
    zmsg_send (&out_zmsg, api_socket);
    out_zmsg = NULL;
    
    
    //
    // wait for the reply
    //
    poller =zpoller_new (api_socket, NULL); 
    reader_socket = zpoller_wait(poller, timeout);  
    zpoller_destroy(&poller);
    if(reader_socket == NULL){   
        //timeout
        ret = ERROR_CODE_TIMEOUT;
        SET_ERR_INFO(err_info, "Request Timeout");   
        goto error_1;            
      
    }
    //receive the reply
    
    in_zmsg = zmsg_recv(api_socket);
    if (!in_zmsg){
        goto error_1; //  Interrupted
    }

    in_frame = zmsg_pop(in_zmsg);
    if(in_frame == NULL){
        goto error_1;  // invalid
    }
    in_blob_frame = zmsg_pop(in_zmsg); 
    
    
    //generate a rpc result to store the reply
    in_result = new MsgRpcResult();
    reply = in_result->mutable_reply();
    in_result->set_blob_frame(in_blob_frame);

    
/*    
    in_frame = zframe_recv(api_socket);
    if(in_frame == NULL){
        ret = ERROR_CODE_GENERAL;
        SET_ERR_INFO(err_info, "NULL Reply");   
        goto error_1;  
    }
*/
    //after used, need free the frame
        
    if(!reply->ParseFromArray(zframe_data(in_frame), zframe_size(in_frame))){
 
        zframe_destroy(&in_frame);  
        ret = ERROR_CODE_GENERAL;
        SET_ERR_INFO(err_info, "Reply Parse Error");  
        
        SAFE_DELETE(in_result);

        goto error_1;
    }

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Receive the following packet from api socket (timestamp:%lld ms):\n",
                (long long)zclock_time());
        fprintf(stderr, "%s\n", reply->DebugString().c_str());
    }
    
    //check reply basic info
    if( reply->header().type() != PROTO_PACKET_TYPE_REPLY ||
        request->header().seq() != reply->header().seq() ||
        request->header().code() != reply->header().code()){
        ret = ERROR_CODE_GENERAL;
        SET_ERR_INFO(err_info, "Received Reply basic info error");   
        SAFE_DELETE(in_result);
        goto error_2;
    }
    
    //successful here  

    (*result) = in_result;
    in_result = NULL;
    
    
error_2:    
    //cache the api socket in receiver
    if(IsInit()){
        pthread_mutex_lock(&lock_); 
        if(last_api_socket_ == NULL){
            last_api_socket_ = api_socket;
            api_socket = NULL;
        }
        pthread_mutex_unlock(&lock_);  
    }

error_1:

    //destroy the socket 
    if(api_socket != NULL){
        zsock_destroy((zsock_t **)&api_socket);
        api_socket = NULL;        
    }
    
    
    if(in_frame != NULL){
        zframe_destroy(&in_frame);
        in_frame = NULL;
    }
    
    if(in_zmsg != NULL){
        zmsg_destroy (&in_zmsg);
        in_zmsg = NULL;        
    } 

    return ret;
  
        
}  

}





