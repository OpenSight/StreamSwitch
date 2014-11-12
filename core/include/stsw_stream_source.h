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
 * stsw_stream_source.h
 *      StreamSource class header file, declare all interfaces of StreamSource.
 * 
 * author: jamken
 * date: 2014-11-8
**/ 


#ifndef STSW_STREAM_SOURCE_H
#define STSW_STREAM_SOURCE_H
#include<map>
#include<stsw_defs.h>
#include<stdint.h>




namespace stream_switch {

class ProtoClientHeartbeatReq;
typedef std::map<int, SourceApiHandlerEntry> SourceApiHanderMap;
struct ClientsInfoType;
typedef void * SocketHandle;
    
class StreamSource{
public:
    StreamSource();
    virtual ~StreamSource();
    
    // The following methods should be invoked by clients
    virtual int init(const std::string &stream_name, int tcp_port);
    virtual void uninit();
    
    virtual void set_stream_meta();
    virtual void get_stream_meta();
    
    virtual int start_api_thread();
    virtual void stop_api_thread();
    
    virtual int send_media_data(void);
    virtual int send_stream_info(void);
    
    virtual void set_stream_state(int new_state);
    
    virtual void register_api_handler(int op_code, SourceApiHandler handler, void * user_data);
    virtual void unregister_api_handler(int op_code);
    
    
        
    // the following methods need client to override
    // to fulfill its functions. They would be invoked
    // by the internal api thread 
        
    // key_frame
    // When the receiver request a key frame, it would be 
    // invoked
    virtual void key_frame(void);
        
    // get_packet_statistic
    // When the control request the statistic info, it would 
    // be invoked for each sub stream
    virtual void get_packet_statistic(int sub_stream_index, uint64_t * expected_packets, uint64_t * actual_packets);


    static int static_metadata_handlertypedef(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int static_key_frame_handler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int static_statistic_handler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int static_client_heartbeat_handler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    

protected:
    int metadata_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    int key_frame_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    int statistic_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    int client_heartbeat_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);

private:
    std::string stream_name_;
    int tcp_port_;
    SocketHandle api_socket_;
    SocketHandle publish_socket_;
    LockHandle lock_;
    ThreadHandle * api_thread_;
    SourceApiHanderMap api_handler_map_;
    
// stream source flags
#define STREAM_SOURCE_FLAG_INIT 1
#define STREAM_SOURCE_FLAG_META_READY 2
    uint32_t flags_;      

    SubStreamMediaStatisticVector staitistic_;
    StreamMetadata stream_meta_;
    uint32_t cur_bps_;
    int64_t last_frame_sec_;
    int32_t last_frame_usec_;
    int stream_state_;
    ReceiversInfoType * receivers_info_;
                             
};
}

#endif