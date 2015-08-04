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
 * stsw_stream_source.cc
 *      StreamSource class implementation file, define all methods of StreamSource.
 * 
 * author: jamken
 * date: 2014-11-8
**/ 

#include <stsw_stream_source.h>
#include <stdint.h>
#include <list>
#include <string.h>
#include <errno.h>

#include <czmq.h>

#include <stsw_lock_guard.h>
#include <stsw_source_listener.h>

#include <pb_packet.pb.h>
#include <pb_client_heartbeat.pb.h>
#include <pb_media.pb.h>
#include <pb_stream_info.pb.h>
#include <pb_metadata.pb.h>
#include <pb_media_statistic.pb.h>
#include <pb_client_list.pb.h>

namespace stream_switch {

    
typedef std::list<ProtoClientHeartbeatReq> ClientHeartbeatList;    
    
struct ReceiversInfoType{
    ClientHeartbeatList receiver_list;
};   

    
StreamSource::StreamSource()
:tcp_port_(0), 
api_socket_(NULL), publish_socket_(NULL), api_thread_id_(0), 
flags_(0), cur_bytes_(0), cur_bps_(0), 
last_frame_sec_(0), last_frame_usec_(0), stream_state_(SOURCE_STREAM_STATE_CONNECTING), 
last_heartbeat_time_(0), listener_(NULL), pub_queue_size_(STSW_PUBLISH_SOCKET_HWM)

{
    receivers_info_ = new ReceiversInfoType();
    
}

StreamSource::~StreamSource()
{
    Uninit();
    SAFE_DELETE(receivers_info_);
}

int StreamSource::Init(const std::string &stream_name, int tcp_port, 
                       uint32_t pub_queue_size, 
                       SourceListener *listener, 
                       uint32_t debug_flags, std::string *err_info)
{
    int ret;
    //params check
    if(stream_name.size() == 0){
        SET_ERR_INFO(err_info, "stream_name cannot be 0");   
        return ERROR_CODE_PARAM;
    }else if(stream_name.size() >= 64){
        SET_ERR_INFO(err_info, "stream_name cannot be larger than 64");   
        return ERROR_CODE_PARAM;        
    }
    if((flags_ & STREAM_SOURCE_FLAG_INIT) != 0){
        SET_ERR_INFO(err_info, "source already init");           
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
            ret = ERROR_CODE_PARAM;
            fprintf(stderr, "socket address too long: %s\n", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            ret = ERROR_CODE_SYSTEM;
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
            ret = ERROR_CODE_PARAM;
            fprintf(stderr, "socket address too long: %s\n", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            ret = ERROR_CODE_SYSTEM;
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }       
    api_socket_ = zsock_new(ZMQ_REP);
    if(api_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new() create api socket failed");   
        fprintf(stderr, "zsock_new() create api socket failed\n");
        goto error_2;            
    }
    zsock_set_linger(api_socket_, 0); //no linger 
    if(zsock_attach((zsock_t *)api_socket_, tmp_addr, true)){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_attach() failed for the api socket: maybe address error");          
        fprintf(stderr, "zsock_attach() failed for the api socket: "
                "maybe address (%s) error\n", tmp_addr);
        goto error_2;         
    }
    
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
            ret = ERROR_CODE_PARAM;
            fprintf(stderr, "socket address too long: %s\n", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            ret = ERROR_CODE_SYSTEM;
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
            ret = ERROR_CODE_PARAM;
            fprintf(stderr, "socket address too long: %s\n", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            ret = ERROR_CODE_SYSTEM;
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }    
    publish_socket_ = zsock_new(ZMQ_PUB);
    if(api_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new create publish socket failed");          
        fprintf(stderr, "zsock_new create publish socket failed\n");
        goto error_2;            
    }    
    //wait for 100 ms to send the remaining packet before close
    zsock_set_linger(publish_socket_, STSW_PUBLISH_SOCKET_LINGER);
 
    zsock_set_sndhwm(publish_socket_, pub_queue_size);   
    zsock_set_rcvhwm(publish_socket_, pub_queue_size);  
    if(zsock_attach((zsock_t *)publish_socket_, tmp_addr, false)){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_attach() failed for the pub socket: maybe address error");          
        fprintf(stderr, "zsock_attach() failed for the pub socket"
                "maybe address (%s) error\n", tmp_addr);
        goto error_2;          
    }
    
    
    //init handlers
    RegisterApiHandler(PROTO_PACKET_CODE_METADATA, (SourceApiHandler)StaticMetadataHandler, this);
    RegisterApiHandler(PROTO_PACKET_CODE_KEY_FRAME, (SourceApiHandler)StaticKeyFrameHandler, this);    
    RegisterApiHandler(PROTO_PACKET_CODE_MEDIA_STATISTIC, (SourceApiHandler)StaticStatisticHandler, this);
    RegisterApiHandler(PROTO_PACKET_CODE_CLIENT_HEARTBEAT, (SourceApiHandler)StaticClientHeartbeatHandler, this);   
    RegisterApiHandler(PROTO_PACKET_CODE_CLIENT_LIST, (SourceApiHandler)StaticClientListHandler, this);       

    //init metadata
    stream_meta_.sub_streams.clear();
    stream_meta_.bps = 0;
    stream_meta_.play_type = STREAM_PLAY_TYPE_LIVE;
    stream_meta_.ssrc = 0;
    stream_meta_.source_proto = "";    
    
    //init statistic 
    statistic_.clear();

    stream_name_ = stream_name;
    tcp_port_ = tcp_port;
    debug_flags_ = debug_flags;
    
    set_listener(listener);
    
    pub_queue_size_ = pub_queue_size;
    
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
//error_1:    
    
    pthread_mutex_destroy(&lock_);   
    
error_0:
    
    return ret;
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

bool StreamSource::IsStarted()
{
     return (flags_ & STREAM_SOURCE_FLAG_STARTED) != 0;   
}
bool StreamSource::IsWaitingReply()
{
    return (flags_ & STREAM_SOURCE_FLAG_WAITING_REPLY) != 0;   
}

void StreamSource::set_stream_meta(const StreamMetadata & stream_meta)
{
    LockGuard guard(&lock_);
    
    //update meta
        
    if(stream_meta_.ssrc != stream_meta.ssrc || 
       stream_meta_.sub_streams.size() != stream_meta.sub_streams.size()){
        //stream source is totaly changed
        
        int sub_stream_num = stream_meta.sub_streams.size();
        
        // clear the statistic
        statistic_.clear();
        statistic_.resize(sub_stream_num);
        int i;
        for(i=0;i<sub_stream_num;i++){
            statistic_[i].sub_stream_index = i;
            statistic_[i].media_type = (SubStreamMediaType)stream_meta.sub_streams[i].media_type;     
        }
        
        if(stream_meta_.ssrc != stream_meta.ssrc){
            cur_bps_ = 0;
            cur_bytes_ = 0;
            last_frame_sec_ = 0;
            last_frame_usec_ = 0;
        }
        
    }else{
        // ssrc is the same, means just update the metadata
        // to supplement more info
        
    }
    
    stream_meta_ = stream_meta;      
    
}

StreamMetadata StreamSource::stream_meta()
{
    LockGuard guard(&lock_);  
    return stream_meta_;
}


int StreamSource::Start(std::string *err_info)
{
    if(!IsInit()){
        SET_ERR_INFO(err_info, "Source not init");          
        return -1;
    }
    
    LockGuard guard(&lock_);      

    if(flags_ & STREAM_SOURCE_FLAG_STARTED){
        //already start
        return 0;        
    }
    if(api_thread_id_ != 0){
        //STREAM_SOURCE_FLAG_STARTED is clear but api_thread_id_
        // is set, means other thread is stopping the source, just
        //return error
        SET_ERR_INFO(err_info, "An other thread is stopping the source, try later");       
        return ERROR_CODE_BUSY;
    }    
    
    flags_ |= STREAM_SOURCE_FLAG_STARTED;    
    
    //start the internal thread
    int ret = pthread_create(&api_thread_id_, NULL, StreamSource::StaticThreadRoutine, this);
    if(ret){
        if(err_info){
            *err_info = "pthread_create failed:";
            //char tmp[64];
            //*err_info += strerror_r(errno, tmp, 64);
            *err_info += strerror(errno);
        }

        perror("Start Source internal thread failed");
        flags_ &= ~(STREAM_SOURCE_FLAG_STARTED); 
        api_thread_id_  = 0;
        return -1;
    }
    
    return 0;

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

    //wait for the thread terminated
    if(api_thread_id_ != 0){
        void * res;
        int ret;
        pthread_t api_thread_id = api_thread_id_;
        pthread_mutex_unlock(&lock_);  
        ret = pthread_join(api_thread_id, &res);
        if (ret != 0){
            perror("Stop Source internal thread failed");
        }
        pthread_mutex_lock(&lock_); 
        api_thread_id_ = 0;      
    }
    
    pthread_mutex_unlock(&lock_);      
}

int StreamSource::SendLiveMediaFrame(const MediaFrameInfo &frame_info, 
                                    const char * frame_data, 
                                    size_t frame_size, 
                                    std::string *err_info)
{
    uint64_t seq;

    if(!IsInit()){
        SET_ERR_INFO(err_info, "Source not init");        
        return ERROR_CODE_GENERAL;
    }
    
    LockGuard guard(&lock_);
    
    // check metadata
    if(stream_meta_.ssrc != frame_info.ssrc){
        SET_ERR_INFO(err_info, "ssrc not match");
        return ERROR_CODE_PARAM;
    }
    // check sub stream index
    if(frame_info.sub_stream_index >= (int32_t)stream_meta_.sub_streams.size()){
        char tmp[64];
        sprintf(tmp, "Sub Stream(%d) Not Found", frame_info.sub_stream_index);
        SET_ERR_INFO(err_info, tmp);
        return ERROR_CODE_PARAM;        
    }
    
    //
    // update the statistic
    //
    cur_bytes_ += frame_size;
    last_frame_sec_ = frame_info.timestamp.tv_sec;
    last_frame_usec_ = frame_info.timestamp.tv_usec;
    
    int sub_stream_index = frame_info.sub_stream_index;
    
    if(frame_info.frame_type == MEDIA_FRAME_TYPE_KEY_FRAME ||
       frame_info.frame_type == MEDIA_FRAME_TYPE_DATA_FRAME){
        //the frames contains media data   
        statistic_[sub_stream_index].data_frames++;
        statistic_[sub_stream_index].data_bytes += frame_size;
        seq = ++statistic_[sub_stream_index].last_seq;
 
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
        //not contain media data
        //seq reuse the last seq of the previous data frame
        seq = statistic_[sub_stream_index].last_seq;
    }//if(frame_type == MEDIA_FRAME_TYPE_KEY_FRAME ||
  
    //
    // pack the frame to pb packet
    //
    ProtoCommonPacket media_msg;
    do {
        ProtoMediaFrameMsg media_info;

        media_info.set_stream_index(frame_info.sub_stream_index);
        media_info.set_sec(frame_info.timestamp.tv_sec);
        media_info.set_usec(frame_info.timestamp.tv_usec);
        media_info.set_frame_type((ProtoMediaFrameType)frame_info.frame_type);
        media_info.set_ssrc(frame_info.ssrc);
        media_info.set_seq(seq);
        //media_info.set_data(frame_data, frame_size);

        media_msg.mutable_header()->set_type(PROTO_PACKET_TYPE_MESSAGE);
        media_msg.mutable_header()->set_code(PROTO_PACKET_CODE_MEDIA);
        media_info.SerializeToString(media_msg.mutable_body());    

        if(debug_flags() & DEBUG_FLAG_DUMP_PUBLISH){
            fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_MEDIA message in SendLiveMediaFrame():\n");
            fprintf(stderr, "%s\n", media_info.DebugString().c_str());
        }
        
    }while(0);//release the media_info memory

    //
    // send out from publish socket
    //
    SendPublishMsg((char *)STSW_PUBLISH_MEDIA_CHANNEL, media_msg, frame_data, frame_size);
        
    return 0;
}


int StreamSource::SendRpcReply(const ProtoCommonPacket &reply, 
                               const char * extra_blob, size_t blob_size, 
                               std::string *err_info)
{
    if(!IsInit()){
        SET_ERR_INFO(err_info, "Source not init");        
        return ERROR_CODE_GENERAL;
    }
    
    if(reply.header().type() != PROTO_PACKET_TYPE_REPLY){
        SET_ERR_INFO(err_info, "Packet Type Not Reply");        
        return ERROR_CODE_PARAM;        
    }
    
    if(((debug_flags() & DEBUG_FLAG_DUMP_API) && 
        (reply.header().code() != PROTO_PACKET_CODE_CLIENT_HEARTBEAT)) ||
        ((debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT) && 
        (reply.header().code() == PROTO_PACKET_CODE_CLIENT_HEARTBEAT))) {
            
        fprintf(stderr, "Send out the following packet (with blob size:%d) to api socket (timestamp:%lld ms):\n",
                (int)blob_size,
                (long long)zclock_time());
        fprintf(stderr, "%s\n", reply.DebugString().c_str());
    }
    
  
    pthread_mutex_lock(&lock_); 
    if(!IsWaitingReply()){
        pthread_mutex_unlock(&lock_); 
        //cannot send back the reply because the state mismatch
        SET_ERR_INFO(err_info, "Not Waiting Reply");        
        return ERROR_CODE_GENERAL;        
    }
    
    flags_ &= ~(STREAM_SOURCE_FLAG_WAITING_REPLY);  
    pthread_mutex_unlock(&lock_); 
    
    // send back the reply
    std::string out_data;
    reply.SerializeToString(&out_data);    
    
    zmsg_t *zmsg = zmsg_new ();
    zmsg_addmem(zmsg, out_data.data(), out_data.size());
    if(extra_blob != NULL && blob_size != 0){
        zmsg_addmem(zmsg, extra_blob, blob_size);
    }
    zmsg_send (&zmsg, api_socket_);    
    
    return 0;

}




void StreamSource::set_stream_state(int stream_state)
{
    if(!IsInit()){
        return;
    }
    int old_stream_state;
    
    {
        LockGuard guard(&lock_);        
        old_stream_state = stream_state_;      
        stream_state_ = stream_state;
    }
    if(old_stream_state != stream_state){
        //state change, send out a stream info msg at once
        SendStreamInfo();
    }

    
}

int StreamSource::stream_state()
{
    return stream_state_;
}

void StreamSource::RegisterApiHandler(int op_code, SourceApiHandler handler, void * user_data)
{
    LockGuard guard(&lock_);   
    
    api_handler_map_[op_code].handler = handler;
    api_handler_map_[op_code].user_data = user_data;
    
}

void StreamSource::UnregisterApiHandler(int op_code)
{
    SourceApiHanderMap::iterator it;
    LockGuard guard(&lock_);   
    
    it = api_handler_map_.find(op_code);
    if(it != api_handler_map_.end()){
        api_handler_map_.erase(it);
    }
}

void StreamSource::UnregisterAllApiHandler()
{
    LockGuard guard(&lock_);       
    api_handler_map_.clear(); 
}

int StreamSource::StaticMetadataHandler(void * user_data, const ProtoCommonPacket &request,
                                        const char * extra_blob, size_t blob_size)
{
    StreamSource * source = (StreamSource * )user_data;
    return source->MetadataHandler(request, extra_blob, blob_size);    
}
int StreamSource::StaticKeyFrameHandler(void * user_data, const ProtoCommonPacket &request,
                                        const char * extra_blob, size_t blob_size)
{
    StreamSource * source = (StreamSource * )user_data;
    return source->KeyFrameHandler(request, extra_blob, blob_size);
}
int StreamSource::StaticStatisticHandler(void * user_data, const ProtoCommonPacket &request,
                                         const char * extra_blob, size_t blob_size)
{
    StreamSource * source = (StreamSource * )user_data;
    return source->StatisticHandler(request, extra_blob, blob_size);
}
int StreamSource::StaticClientHeartbeatHandler(void * user_data, const ProtoCommonPacket &request,
                                               const char * extra_blob, size_t blob_size)
{
    StreamSource * source = (StreamSource * )user_data;
    return source->ClientHeartbeatHandler(request, extra_blob, blob_size);
}

int StreamSource::StaticClientListHandler(void * user_data, const ProtoCommonPacket &request,
                                          const char * extra_blob, size_t blob_size)
{
    StreamSource * source = (StreamSource * )user_data;
    return source->ClientListHandler(request, extra_blob, blob_size);
}

    
int StreamSource::MetadataHandler(const ProtoCommonPacket &request,
                                  const char * extra_blob, size_t blob_size)
{

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode no body from a PROTO_PACKET_CODE_METADATA request\n");
    } 
    
    ProtoMetaRep metadata;    
    
    {
        LockGuard guard(&lock());

        metadata.set_play_type((ProtoPlayType)stream_meta_.play_type);
        metadata.set_source_proto(stream_meta_.source_proto);
        metadata.set_ssrc(stream_meta_.ssrc);
        metadata.set_bps(stream_meta_.bps);

        SubStreamMetadataVector::iterator it;
        int32_t index;
        for(it = stream_meta_.sub_streams.begin(), 
            index = 0; 
            it != stream_meta_.sub_streams.end();
            it++, index++){
            ProtoSubStreamInfo * sub_stream = 
                metadata.add_sub_streams();
            sub_stream->set_index(index);
            sub_stream->set_media_type((ProtoSubStreamMediaType)it->media_type);
            sub_stream->set_codec_name(it->codec_name);
            sub_stream->set_direction((ProtoSubStreamDirectionType)it->direction);
            sub_stream->set_extra_data(it->extra_data);
            switch(it->media_type){
                case SUB_STREAM_MEIDA_TYPE_VIDEO:
                {
                    sub_stream->set_height(it->media_param.video.height);
                    sub_stream->set_width(it->media_param.video.width);   
                    sub_stream->set_fps(it->media_param.video.fps);
                    sub_stream->set_gov(it->media_param.video.gov);  
                    break;                      
                }

                case SUB_STREAM_MEIDA_TYPE_AUDIO:
                {
                        
                    sub_stream->set_samples_per_second(it->media_param.audio.samples_per_second);
                    sub_stream->set_channels(it->media_param.audio.channels);  
                    sub_stream->set_bits_per_sample(it->media_param.audio.bits_per_sample);
                    sub_stream->set_sampele_per_frame(it->media_param.audio.sampele_per_frame);                        
                    break;
                        
                }
                    
                case SUB_STREAM_MEIDA_TYPE_TEXT:
                {
                        
                    sub_stream->set_x(it->media_param.text.x);
                    sub_stream->set_y(it->media_param.text.y);  
                    sub_stream->set_fone_size(it->media_param.text.fone_size);
                    sub_stream->set_font_type(it->media_param.text.font_type);                        
                    break;
                        
                }

                default:
                    break;
            }
        }// for(it = stream_meta_.sub_streams.begin();  
    }
    


    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_METADATA reply:\n");
        fprintf(stderr, "%s\n", metadata.DebugString().c_str());
    }  
    
    ProtoCommonPacket reply;
    reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);
    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply.mutable_header()->set_info(""); 
    reply.mutable_header()->set_code(request.header().code());   
    reply.mutable_header()->set_seq(request.header().seq());    
    metadata.SerializeToString(reply.mutable_body());

    
    //send back the reply
    SendRpcReply(reply, NULL, 0, NULL);
  
    return 0;
}

int StreamSource::KeyFrameHandler(const ProtoCommonPacket &request, 
                                  const char * extra_blob, size_t blob_size)
{
 
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode no body from a PROTO_PACKET_CODE_KEY_FRAME request\n");
    }     
    SourceListener *plistener = listener();
    if(plistener != NULL){
        plistener->OnKeyFrame();
    }
    
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode no body from a PROTO_PACKET_CODE_KEY_FRAME reply\n");
    }   
    
    ProtoCommonPacket reply;
    reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);    
    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply.mutable_header()->set_info(""); 
    reply.mutable_header()->set_code(request.header().code());   
    reply.mutable_header()->set_seq(request.header().seq()); 
    //send back the reply
    SendRpcReply(reply, NULL, 0, NULL);
 
    return 0;
}

int StreamSource::StatisticHandler(const ProtoCommonPacket &request,
                                   const char * extra_blob, size_t blob_size)
{

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode no body from a PROTO_PACKET_CODE_MEDIA_STATISTIC request\n");
    }    
    
    MediaStatisticInfo local_statistic;
    {
        LockGuard guard(&lock());
        local_statistic.sub_streams = statistic_;
        local_statistic.ssrc = stream_meta_.ssrc;
    }
    
    local_statistic.timestamp = (int64_t)zclock_time();
    local_statistic.sum_bytes = 0;
    SubStreamMediaStatisticVector::iterator it;
    for(it = local_statistic.sub_streams.begin(); 
        it != local_statistic.sub_streams.end();
        it++){
        local_statistic.sum_bytes += it->data_bytes;
    }
    
    //invoke the user function to overwrite local_statistic
    SourceListener *plistener = listener();
    if(plistener != NULL){
        plistener->OnMediaStatistic(&local_statistic);
    }
        
    ProtoMediaStatisticRep statistic;

    statistic.set_timestamp(local_statistic.timestamp);
    statistic.set_ssrc(local_statistic.ssrc);
    statistic.set_sum_bytes(local_statistic.sum_bytes); 
    
    for(it = local_statistic.sub_streams.begin(); 
        it != local_statistic.sub_streams.end();
        it++){
        ProtoSubStreamMediaStatistic * sub_stream_stat = 
            statistic.add_sub_stream_stats();                
            
        sub_stream_stat->set_sub_stream_index(it->sub_stream_index);
        sub_stream_stat->set_media_type((ProtoSubStreamMediaType)it->media_type);
        sub_stream_stat->set_data_bytes(it->data_bytes);
        sub_stream_stat->set_key_bytes(it->key_bytes);
        sub_stream_stat->set_data_frames(it->data_frames);
        sub_stream_stat->set_key_frames(it->key_frames);
        sub_stream_stat->set_lost_frames(it->lost_frames);
        sub_stream_stat->set_last_gov(it->last_gov);            
    }// for(it = stream_meta_.sub_streams.begin();  

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_MEDIA_STATISTIC reply:\n");
        fprintf(stderr, "%s\n", statistic.DebugString().c_str());
    }
                
    ProtoCommonPacket reply;
    reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);    
    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply.mutable_header()->set_info(""); 
    reply.mutable_header()->set_code(request.header().code());   
    reply.mutable_header()->set_seq(request.header().seq());  
    statistic.SerializeToString(reply.mutable_body());      
    //send back the reply
    SendRpcReply(reply, NULL, 0, NULL);
    
    return 0;
}

int StreamSource::ClientHeartbeatHandler(const ProtoCommonPacket &request,
                                         const char * extra_blob, size_t blob_size)
{
    

    ProtoCommonPacket reply;
    reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);
    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply.mutable_header()->set_info(""); 
    reply.mutable_header()->set_code(request.header().code());   
    reply.mutable_header()->set_seq(request.header().seq());     
    
    ProtoClientHeartbeatReq client_heartbeat;
    if(client_heartbeat.ParseFromString(request.body())){
        int64_t now = time(NULL);
        client_heartbeat.set_last_active_time(now);
        
        if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
            fprintf(stderr, "Decode the following body from a PROTO_PACKET_CODE_CLIENT_HEARTBEAT request:\n");
            fprintf(stderr, "%s\n", client_heartbeat.DebugString().c_str());
        }        
        
        {
            LockGuard guard(&lock());
            
            //
            // find the client
            //
            ClientHeartbeatList::iterator it;
            bool found = false;
            for(it = receivers_info_->receiver_list.begin(); 
                it != receivers_info_->receiver_list.end();
                it++){
                if(client_heartbeat.client_ip() == it->client_ip() &&
                   client_heartbeat.client_port() == it->client_port() &&
                   client_heartbeat.client_token() == it->client_token()){
                    found = true;
                    break;                                           
                }
            }
            if(found){
                //client already exit, just update it's active time.
                *it = client_heartbeat;
              
                
            }else{
                //client no exist, check the max number and add
                if(receivers_info_->receiver_list.size() < STSW_MAX_CLIENT_NUM){
                    receivers_info_->receiver_list.push_back(client_heartbeat); 
                }else{
                    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_BAD_REQUEST);
                    reply.mutable_header()->set_info("ProtoClientHeartbeatReq body Parse Error"); 
                    
                    
                }
            }
            
        }
        
        
        if(reply.header().status() == PROTO_PACKET_STATUS_OK){
            ProtoClientHeartbeatRep reply_info;
            reply_info.set_timestamp(now);
            reply_info.set_lease(STSW_CLIENT_LEASE);
            reply_info.SerializeToString(reply.mutable_body());  

            if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
                fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_CLIENT_HEARTBEAT reply:\n");
                fprintf(stderr, "%s\n", reply_info.DebugString().c_str());
            }         
        }
 

  

                    
    }else{
        reply.mutable_header()->set_status(PROTO_PACKET_STATUS_BAD_REQUEST);
        reply.mutable_header()->set_info("ProtoClientHeartbeatReq body Parse Error");           
    }

    //send back the reply
    SendRpcReply(reply, NULL, 0, NULL);
    
    return 0;
}


int StreamSource::ClientListHandler(const ProtoCommonPacket &request, 
                                    const char * extra_blob, size_t blob_size)
{

    
    ProtoCommonPacket reply;
    reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);
    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply.mutable_header()->set_info(""); 
    reply.mutable_header()->set_code(request.header().code());   
    reply.mutable_header()->set_seq(request.header().seq());     
    
        
    ProtoClientListReq client_list_req;
    ProtoClientListRep client_list_rep;
    if(client_list_req.ParseFromString(request.body())){
        
        if(debug_flags() & DEBUG_FLAG_DUMP_API){
            fprintf(stderr, "Decode the following body from a PROTO_PACKET_CODE_CLIENT_LIST request:\n");
            fprintf(stderr, "%s\n", client_list_req.DebugString().c_str());
        }        
        
        {
            LockGuard guard(&lock());
            
            client_list_rep.set_total_num(receivers_info_->receiver_list.size());
            client_list_rep.set_start_index(client_list_req.start_index());
            
            if(client_list_req.start_index() < receivers_info_->receiver_list.size()){
                ClientHeartbeatList::iterator it = 
                    receivers_info_->receiver_list.begin();
                std::advance(it, client_list_req.start_index());                
                uint32_t client_num = client_list_req.client_num();
 

                while(it != receivers_info_->receiver_list.end() 
                      && client_num > 0){                    
                    ProtoClientHeartbeatReq * client_info = 
                        client_list_rep.add_client_list();   
                    *client_info = *it;
                    
                    it++;  
                    client_num--;
                }
            }

        }
        
  
        client_list_rep.SerializeToString(reply.mutable_body());     

        if(debug_flags() & DEBUG_FLAG_DUMP_API){
            fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_CLIENT_LIST reply:\n");
            fprintf(stderr, "%s\n", client_list_rep.DebugString().c_str());
        } 
                    
    }else{
        reply.mutable_header()->set_status(PROTO_PACKET_STATUS_BAD_REQUEST);
        reply.mutable_header()->set_info("ProtoClientHeartbeatReq body Parse Error");           
    }

    //send back the reply
    SendRpcReply(reply, NULL, 0, NULL);
    
    return 0;
}

void StreamSource::OnApiSocketRead()
{
    zframe_t * in_frame = NULL, *blob_frame = NULL;
    const char * extra_blob = NULL;
    size_t blob_size = 0;
    ProtoCommonPacket request;

    
    zmsg_t *zmsg = zmsg_recv(api_socket_);
    if (!zmsg){
        return;  // no frame receive
    }
    in_frame = zmsg_pop(zmsg);
    if(in_frame == NULL){
        //invalid, nothing can do        
    }
    blob_frame = zmsg_pop(zmsg);
    if(blob_frame != NULL){
        extra_blob = (const char *)zframe_data(blob_frame);
        blob_size = zframe_size(blob_frame);
        if(blob_size == 0 && extra_blob != NULL){ //check
            extra_blob = NULL;
        }
    }
        
            
    if(request.ParseFromArray(zframe_data(in_frame), zframe_size(in_frame))){
             
        if(((debug_flags() & DEBUG_FLAG_DUMP_API) && 
            (request.header().code() != PROTO_PACKET_CODE_CLIENT_HEARTBEAT)) ||
            ((debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT) && 
            (request.header().code() == PROTO_PACKET_CODE_CLIENT_HEARTBEAT))) {
            
            fprintf(stderr, "Receive the following packet (with blob size:%d) from api socket (timestamp:%lld ms):\n",
                    (int)blob_size, 
                    (long long)zclock_time());
            fprintf(stderr, "%s\n", request.DebugString().c_str());
        }
           
        OnRpcRequest(request, extra_blob, blob_size); //OnRpcRequest would send out a reply surely.
        
    }else{
        ProtoCommonPacket err_reply;
        err_reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);
        err_reply.mutable_header()->set_status(PROTO_PACKET_STATUS_BAD_REQUEST);
        err_reply.mutable_header()->set_info("Request Parse Error");    
        SendRpcReply(err_reply, NULL, 0, NULL); 
    }

    //after used, need free the frame
    if(blob_frame != NULL) {
        zframe_destroy(&blob_frame);
        blob_frame = NULL;
    }    
    
    if(in_frame != NULL){
        zframe_destroy(&in_frame);
        in_frame = NULL;
    }
    
    if(zmsg != NULL){
        zmsg_destroy (&zmsg);
        zmsg = NULL;        
    }     

    
    return;   
} 

// OnRpcRequest ensure that a reply for the given request 
// must be send back by SendRpcReply()
void StreamSource::OnRpcRequest(const ProtoCommonPacket &request,
                                const char * extra_blob, size_t blob_size)
{
    ProtoCommonPacket default_reply;
    default_reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);
    default_reply.mutable_header()->set_status(PROTO_PACKET_STATUS_INTERNAL_ERR);
    default_reply.mutable_header()->set_info("Handler Not Send Reply");
    default_reply.mutable_header()->set_code(request.header().code());   
    default_reply.mutable_header()->set_seq(request.header().seq());
    
    int op_code = request.header().code();
    SourceApiHanderMap::iterator it;
    pthread_mutex_lock(&lock_); 
        
    it = api_handler_map_.find(op_code);
    if(it == api_handler_map_.end()){  
        pthread_mutex_unlock(&lock_); 
        default_reply.mutable_header()->set_status(PROTO_PACKET_STATUS_INTERNAL_ERR);
        char err_info[64];
        sprintf(err_info, "Handler for code(%d) Not Found", op_code);
        default_reply.mutable_header()->set_info(err_info);               
        //send back the default reply
        SendRpcReply(default_reply, NULL, 0, NULL);              
    }else{
        SourceApiHandlerEntry entry = it->second;
        flags_ |= STREAM_SOURCE_FLAG_WAITING_REPLY;
        pthread_mutex_unlock(&lock_); 
        
        entry.handler(entry.user_data, request, extra_blob, blob_size); 

        if(IsWaitingReply()){
            //handler forget to send reply
            default_reply.mutable_header()->set_status(PROTO_PACKET_STATUS_INTERNAL_ERR);
            default_reply.mutable_header()->set_info("API Handler Error (Not send reply)");       
            //send back the default reply
            SendRpcReply(default_reply, NULL, 0, NULL); 
        }
    }//if(it == api_handler_map_.end()){     
}

int StreamSource::Heartbeat(int64_t now)
{
    if(now == 0){
        now = zclock_mono();
    }

    LockGuard guard(&lock_);
    
    int64_t elapse = now - last_heartbeat_time_;
    last_heartbeat_time_ = now;   
    if(elapse != 0){
        cur_bps_ = cur_bytes_ * 8 * 1000 / elapse;
        cur_bytes_ = 0;
    }
    
    int64_t now_sec = time(NULL);
    
    //
    // kick out the timeout client
    //
    ClientHeartbeatList::iterator it;
    for(it = receivers_info_->receiver_list.begin(); 
        it != receivers_info_->receiver_list.end();){
        if(now_sec - it->last_active_time() >= STSW_CLIENT_LEASE){
            //the client timeout
            it = receivers_info_->receiver_list.erase(it);
        }else{
            it++;
        }
    }    
   
    SendStreamInfo(); // publish the stream info at heartbeat interval    
    return 0;    
}

void * StreamSource::StaticThreadRoutine(void *arg)
{
    StreamSource * source = (StreamSource * )arg;
    source->InternalRoutine();
    return NULL;
}
    
void StreamSource::InternalRoutine()
{
    zpoller_t  * poller =zpoller_new (api_socket_, NULL);
    int64_t next_heartbeat_time = zclock_mono() + 
        STSW_STREAM_SOURCE_HEARTBEAT_INT;
    
#define MAX_POLLER_WAIT_TIME    100
    
    while(IsStarted()){
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
            OnApiSocketRead();
        }
                     
        // check for heartbeat
        now = zclock_mono();
        if(now >= next_heartbeat_time){            
            Heartbeat(now);            
            //calculate next heartbeat time
            do{
                next_heartbeat_time += STSW_STREAM_SOURCE_HEARTBEAT_INT;
            }while(next_heartbeat_time <= now);            
        }         
    }
    
    if(poller != NULL){
        zpoller_destroy (&poller);
    }

}    
    
void StreamSource::SendStreamInfo(void)
{
    LockGuard guard(&lock_);
    ProtoStreamInfoMsg stream_info;
    ProtoCommonPacket info_msg;
    
    stream_info.set_state((ProtoSourceStreamState )stream_state_);
    stream_info.set_play_type((ProtoPlayType)stream_meta_.play_type);
    stream_info.set_ssrc(stream_meta_.ssrc);
    stream_info.set_source_proto(stream_meta_.source_proto);
    stream_info.set_cur_bps(cur_bps_);
    stream_info.set_last_frame_sec(last_frame_sec_);
    stream_info.set_last_frame_usec(last_frame_usec_);
    stream_info.set_stream_name(stream_name_);
    stream_info.set_client_num((int32_t)receivers_info_->receiver_list.size());
    
    info_msg.mutable_header()->set_type(PROTO_PACKET_TYPE_MESSAGE);
    info_msg.mutable_header()->set_code(PROTO_PACKET_CODE_STREAM_INFO);
    stream_info.SerializeToString(info_msg.mutable_body());   

    if(debug_flags() & DEBUG_FLAG_DUMP_PUBLISH){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_STREAM_INFO message in SendStreamInfo():\n");
        fprintf(stderr, "%s\n", stream_info.DebugString().c_str());
    }  
    
    //
    // send out from publish socket
    //
    SendPublishMsg((char *)STSW_PUBLISH_INFO_CHANNEL, info_msg, NULL, 0);

}

//Before invoke SendPublishMsg(), the internal lock must be hold first.
void StreamSource::SendPublishMsg(char * channel_name, const ProtoCommonPacket &msg, 
                                  const char * extra_blob, size_t blob_size)
{
    if(channel_name == NULL || strlen(channel_name) == 0){
        //no channel, just ignore
        return;
    }
    if(!IsInit()){
        //if uninit, just ignore
        return;
    }    

    if(debug_flags() & DEBUG_FLAG_DUMP_PUBLISH){
        fprintf(stderr, "Send out the following packet (with blob size: %d) into publish socket channel %s (timestamp:%lld ms):\n", 
                (int)blob_size,
                channel_name, 
                (long long)zclock_time());
        fprintf(stderr, "%s\n", msg.DebugString().c_str());
    }

    //LockGuard guard(&lock_); //no need to lock again
    
    std::string packet_data;
    msg.SerializeToString(&packet_data);
    
    zmsg_t *zmsg = zmsg_new ();
    zmsg_addstr(zmsg, channel_name);
    zmsg_addmem(zmsg, packet_data.data(), packet_data.size());
    if(extra_blob != NULL && blob_size != 0){
        zmsg_addmem(zmsg, extra_blob, blob_size);
    }
    zmsg_send (&zmsg, publish_socket_);
    
    
}

}





