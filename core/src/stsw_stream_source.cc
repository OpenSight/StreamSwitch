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

#include <stsw_stream_source.h>
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

    
typedef std::list<ProtoClientHeartbeatReq> ClientHeartbeatList;    
    
struct ReceiversInfoType{
    ClientHeartbeatList receiver_list;
};   

    
StreamSource::StreamSource()
:tcp_port_(0), 
api_socket_(NULL), publish_socket_(NULL), api_thread_id_(0), 
flags_(0), cur_bytes_(0), cur_bps_(0), 
last_frame_sec_(0), last_frame_usec_(0), stream_state_(SOURCE_STREAM_STATE_CONNECTING), 
last_heartbeat_time_(0)
{
    receivers_info_ = new ReceiversInfoType();
    
}

StreamSource::~StreamSource()
{
    SAFE_DELETE(receivers_info_);
}

int StreamSource::Init(const std::string &stream_name, int tcp_port, 
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
            fprintf(stderr, "socket address too long: %s", tmp_addr);
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
            fprintf(stderr, "socket address too long: %s", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            ret = ERROR_CODE_SYSTEM;
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }       
    api_socket_ = zsock_new_rep(tmp_addr);
    if(api_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new_rep create api socket failed: maybe address conflict");   
        fprintf(stderr, "zsock_new_rep create api socket failed: maybe address (%s) conflict", tmp_addr);
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
            ret = ERROR_CODE_PARAM;
            fprintf(stderr, "socket address too long: %s", tmp_addr);
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
            fprintf(stderr, "socket address too long: %s", tmp_addr);
            goto error_2;            
        }else if(ret < 0){
            ret = ERROR_CODE_SYSTEM;
            perror("snprintf for socket address failed");
            goto error_2;              
        }
        
    }    
    publish_socket_ = zsock_new_pub(tmp_addr);
    if(api_socket_ == NULL){
        ret = ERROR_CODE_SYSTEM;
        SET_ERR_INFO(err_info, "zsock_new_pub create publish socket failed: maybe address conflict");          
        fprintf(stderr, "zsock_new_pub create publish socket failed: "
                "maybe address (%s) conflict", tmp_addr);
        goto error_2;            
    }    
    //wait for 100 ms to send the remaining packet before close
    zsock_set_linger(publish_socket_, STSW_PUBLISH_SOCKET_LINGER);
 
    zsock_set_sndhwm(publish_socket_, STSW_PUBLISH_SOCKET_HWM);   
    
    //init handlers
    RegisterApiHandler(PROTO_PACKET_CODE_METADATA, (SourceApiHandler)StaticMetadataHandler, NULL);
    RegisterApiHandler(PROTO_PACKET_CODE_KEY_FRAME, (SourceApiHandler)StaticKeyFrameHandler, NULL);    
    RegisterApiHandler(PROTO_PACKET_CODE_MEDIA_STATISTIC, (SourceApiHandler)StaticStatisticHandler, NULL);
    RegisterApiHandler(PROTO_PACKET_CODE_CLIENT_HEARTBEAT, (SourceApiHandler)StaticClientHeartbeatHandler, NULL);   

    //init metadata
    stream_meta_.sub_streams.clear();
    stream_meta_.bps = 0;
    stream_meta_.play_type = Stream_PLAY_TYPE_LIVE;
    stream_meta_.ssrc = 0;
    stream_meta_.source_proto = "";    
    
    //init statistic 
    statistic_.clear();

    stream_name_ = stream_name;
    tcp_port_ = tcp_port;
    debug_flags_ = debug_flags;

    
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

void StreamSource::set_stream_meta(const StreamMetadata & stream_meta)
{
    LockGuard guard(&lock_);
    
    //update meta
    stream_meta_ = stream_meta;       
    
    int sub_stream_num = stream_meta.sub_streams.size();
    
    // clear the statistic
    statistic_.clear();
    statistic_.resize(sub_stream_num);
    int i;
    for(i=0;i<sub_stream_num;i++){
        statistic_[i].sub_stream_index = i;
        statistic_[i].media_type = (SubStreamMediaType)stream_meta.sub_streams[i].media_type;     
    }
    
    cur_bps_ = 0;
    cur_bytes_ = 0;
    last_frame_sec_ = 0;
    last_frame_usec_ = 0;

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
    int ret = pthread_create(&api_thread_id_, NULL, ThreadRoutine, this);
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


int StreamSource::SendLiveMediaFrame(const MediaDataFrame &media_frame, 
                                     std::string *err_info)
{
    uint64_t seq;

    if(!IsInit()){
        SET_ERR_INFO(err_info, "Source not init");        
        return ERROR_CODE_GENERAL;
    }
    
    LockGuard guard(&lock_);
    
    // check metadata
    if(stream_meta_.ssrc != media_frame.ssrc){
        SET_ERR_INFO(err_info, "ssrc not match");
        return ERROR_CODE_PARAM;
    }
    // check sub stream index
    if(media_frame.sub_stream_index >= (int32_t)stream_meta_.sub_streams.size()){
        char tmp[64];
        sprintf(tmp, "Sub Stream(%d) Not Found", media_frame.sub_stream_index);
        SET_ERR_INFO(err_info, tmp);
        return ERROR_CODE_PARAM;        
    }
    
    //
    // update the statistic
    //
    cur_bytes_ += media_frame.data.size();
    last_frame_sec_ = media_frame.timestamp.tv_sec;
    last_frame_usec_ = media_frame.timestamp.tv_usec;
    
    int sub_stream_index = media_frame.sub_stream_index;
    
    if(media_frame.frame_type == MEDIA_FRAME_TYPE_KEY_FRAME ||
       media_frame.frame_type == MEDIA_FRAME_TYPE_DATA_FRAME){
        //the frames contains media data   
        statistic_[sub_stream_index].data_frames++;
        statistic_[sub_stream_index].data_bytes += media_frame.data.size();
        seq = ++statistic_[sub_stream_index].last_seq;
 
        if(media_frame.frame_type == MEDIA_FRAME_TYPE_KEY_FRAME){
            statistic_[sub_stream_index].key_frames ++;
            statistic_[sub_stream_index].key_bytes += media_frame.data.size();
            
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
    ProtoMediaFrameMsg media_info;
    ProtoCommonPacket media_msg;
    media_info.set_stream_index(media_frame.sub_stream_index);
    media_info.set_sec(media_frame.timestamp.tv_sec);
    media_info.set_usec(media_frame.timestamp.tv_usec);
    media_info.set_frame_type((ProtoMediaFrameType)media_frame.frame_type);
    media_info.set_ssrc(media_frame.ssrc);
    media_info.set_seq(seq);
    media_info.set_data(media_frame.data);

    media_msg.mutable_header()->set_type(PROTO_PACKET_TYPE_MESSAGE);
    media_msg.mutable_header()->set_code(PROTO_PACKET_CODE_MEDIA);
    media_info.SerializeToString(media_msg.mutable_body());    

    if(debug_flags() & DEBUG_FLAG_DUMP_PUBLISH){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_MEDIA message in SendLiveMediaFrame():\n");
        fprintf(stderr, "%s\n", media_info.DebugString().c_str());
    }     
    
    //
    // send out from publish socket
    //
    SendPublishMsg((char *)STSW_PUBLISH_MEDIA_CHANNEL, media_msg);
        
    return 0;
}


void StreamSource::set_stream_state(int stream_state)
{
    if(!IsInit()){
        return;
    }

    LockGuard guard(&lock_);        

    int old_stream_state = stream_state_;
    stream_state_ = stream_state;
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


void StreamSource::OnKeyFrame(void)
{
    //default nothing to do
    //need child class to implement this function
}

void OnMediaStatistic(SubStreamMediaStatisticVector *statistic)
{
    //default nothing to do
    //lost frames number is 0 by default
    return;
/*    
    if(statistic == NULL){
        return;
    }
    SubStreamMediaStatisticVector::iterator it;
    for(it = statistic->begin(); 
        it != statistic->end();
        it++){
        it->lost_frames = 0;
    }
*/    
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
    if(request == NULL || reply == NULL){
        //nothing to do
        return 0;
    }
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
        for(it = stream_meta_.sub_streams.begin(); 
            it != stream_meta_.sub_streams.end();
            it++){
            ProtoSubStreamInfo * sub_stream = 
                metadata.add_sub_streams();
            sub_stream->set_index(it->sub_stream_index);
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

    reply->mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply->mutable_header()->set_info(""); 
    metadata.SerializeToString(reply->mutable_body());


    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_METADATA reply:\n");
        fprintf(stderr, "%s\n", metadata.DebugString().c_str());
    }  
  
    return 0;
}
int StreamSource::KeyFrameHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    if(request == NULL || reply == NULL){
        //nothing to do
        return 0;
    }   
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode no body from a PROTO_PACKET_CODE_KEY_FRAME request\n");
    }     
    OnKeyFrame();
    reply->mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply->mutable_header()->set_info("");  

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode no body from a PROTO_PACKET_CODE_KEY_FRAME reply\n");
    }    
    return 0;
}
int StreamSource::StatisticHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    if(request == NULL || reply == NULL){
        //nothing to do
        return 0;
    }
    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Decode no body from a PROTO_PACKET_CODE_MEDIA_STATISTIC request\n");
    }    
    
    SubStreamMediaStatisticVector local_statistic;
    uint32_t ssrc;    
    {
        LockGuard guard(&lock());
        local_statistic = statistic_;
        ssrc = stream_meta_.ssrc;
    }
    
    ProtoMediaStatisticRep statistic;

    statistic.set_timestamp((int64_t)time(NULL));
    statistic.set_ssrc(stream_meta_.ssrc);
    uint64_t sum_bytes = 0;
    int i;
    
    SubStreamMediaStatisticVector::iterator it;
    for(it = local_statistic.begin(), i= 0; 
        it != local_statistic.end();
        it++, i++){
        ProtoSubStreamMediaStatistic * sub_stream_stat = 
            statistic.add_sub_stream_stats();                
            
        sub_stream_stat->set_sub_stream_index(it->sub_stream_index);
        sub_stream_stat->set_media_type((ProtoSubStreamMediaType)it->media_type);
        sub_stream_stat->set_total_bytes(it->data_bytes);
        sub_stream_stat->set_key_bytes(it->key_bytes);
        sub_stream_stat->set_total_frames(it->data_frames);
        sub_stream_stat->set_key_frames(it->key_frames);
        sub_stream_stat->set_expected_frames(it->expected_frames);
        sub_stream_stat->set_last_gov(it->last_gov);
        sum_bytes += it->data_bytes;
            
    }// for(it = stream_meta_.sub_streams.begin();  
    statistic.set_sum_bytes(sum_bytes);
                
    reply->mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
    reply->mutable_header()->set_info(""); 
    statistic.SerializeToString(reply->mutable_body());      

    if(debug_flags() & DEBUG_FLAG_DUMP_API){
        fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_MEDIA_STATISTIC reply:\n");
        fprintf(stderr, "%s\n", statistic.DebugString().c_str());
    }
    
    return 0;
}
int StreamSource::ClientHeartbeatHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data)
{
    if(request == NULL || reply == NULL){
        //nothing to do
        return 0;
    }
    ProtoClientHeartbeatReq client_heartbeat;
    if(client_heartbeat.ParseFromString(request->body())){
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
                   client_heartbeat.client_port() == it->client_port()){
                    //TODO(jamken): check client token
                    found = true;
                    break;                                           
                }
            }
            if(found){
                //client already exit, just update it's active time.
                *it = client_heartbeat;
            }else{
                //client no exist
                receivers_info_->receiver_list.push_back(client_heartbeat);
            }
            
        }
        ProtoClientHeartbeatRep reply_info;
        reply_info.set_timestamp(now);
        reply_info.set_lease(STSW_CLIENT_LEASE);
        
        reply->mutable_header()->set_status(PROTO_PACKET_STATUS_OK);
        reply->mutable_header()->set_info("");  
        reply_info.SerializeToString(reply->mutable_body());     

        if(debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT){
            fprintf(stderr, "Encode the following body into a PROTO_PACKET_CODE_CLIENT_HEARTBEAT reply:\n");
            fprintf(stderr, "%s\n", reply_info.DebugString().c_str());
        } 
                    
    }else{
        reply->mutable_header()->set_status(PROTO_PACKET_STATUS_BAD_REQUEST);
        reply->mutable_header()->set_info("ProtoClientHeartbeatReq body Parse Error");           
    }
    
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
    //after used, need free the frame
    zframe_destroy(&in_frame);    
    
    ProtoCommonPacket request;
    ProtoCommonPacket reply;
    
    // initialize the reply;
    reply.mutable_header()->set_type(PROTO_PACKET_TYPE_REPLY);
    reply.mutable_header()->set_status(PROTO_PACKET_STATUS_INTERNAL_ERR);
    reply.mutable_header()->set_info("Unknown Error");
            
    if(request.ParseFromString(in_data)){
             
        if(((debug_flags() & DEBUG_FLAG_DUMP_API) && 
            (reply.header().code() != PROTO_PACKET_CODE_CLIENT_HEARTBEAT)) ||
            ((debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT) && 
            (reply.header().code() == PROTO_PACKET_CODE_CLIENT_HEARTBEAT))) {
            
            fprintf(stderr, "Receive the following packet from api socket (timestamp:%lld ms):\n",
                    (long long)zclock_time());
            fprintf(stderr, "%s\n", request.DebugString().c_str());
        }
                   
        reply.mutable_header()->set_status(PROTO_PACKET_STATUS_INTERNAL_ERR);
        reply.mutable_header()->set_info("Unknown Error");        
        reply.mutable_header()->set_code(request.header().code());   
        reply.mutable_header()->set_seq(request.header().seq());
        int op_code = request.header().code();
        SourceApiHanderMap::iterator it;
        pthread_mutex_lock(&lock_); 
        it = api_handler_map_.find(op_code);
        if(it == api_handler_map_.end()){
            pthread_mutex_unlock(&lock_); 
            reply.mutable_header()->set_status(PROTO_PACKET_STATUS_INTERNAL_ERR);
            char err_info[64];
            sprintf(err_info, "Handler for code(%d) Not Found", op_code);
            reply.mutable_header()->set_info(err_info);               
            
        }else{
            SourceApiHandlerEntry entry = it->second;
            pthread_mutex_unlock(&lock_); 
            int ret = 0;
            ret = entry.handler(this, &request, &reply, entry.user_data);            
        }          
        
    }else{
        reply.mutable_header()->set_status(PROTO_PACKET_STATUS_BAD_REQUEST);
        reply.mutable_header()->set_info("Request Parse Error");        
    }

    if(((debug_flags() & DEBUG_FLAG_DUMP_API) && 
        (reply.header().code() != PROTO_PACKET_CODE_CLIENT_HEARTBEAT)) ||
        ((debug_flags() & DEBUG_FLAG_DUMP_HEARTBEAT) && 
        (reply.header().code() == PROTO_PACKET_CODE_CLIENT_HEARTBEAT))) {
            
        fprintf(stderr, "Send out the following packet to api socket (timestamp:%lld ms):\n",
                (long long)zclock_time());
        fprintf(stderr, "%s\n", request.DebugString().c_str());
    }

    
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
    
    ClientHeartbeatList::iterator it;
    for(it = receivers_info_->receiver_list.begin(); 
        it != receivers_info_->receiver_list.end();
        it ++){
        ProtoClientHeartbeatReq * new_client = 
            stream_info.add_clients();
        *new_client = *it;            
    }

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
    SendPublishMsg((char *)STSW_PUBLISH_INFO_CHANNEL, info_msg);

}


void StreamSource::SendPublishMsg(char * channel_name, const ProtoCommonPacket &msg)
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
        fprintf(stderr, "Send out the following packet into publish socket channel %s (timestamp:%lld ms):\n", 
                channel_name, 
                (long long)zclock_time());
        fprintf(stderr, "%s\n", msg.DebugString().c_str());
    }

    LockGuard guard(&lock_);
    
    std::string packet_data;
    msg.SerializeToString(&packet_data);

    zsock_send(publish_socket_, "sb",  
               channel_name, 
               packet_data.data(), packet_data.size());         
    
}

}





