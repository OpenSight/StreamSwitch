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
#include<czmq.h>
#include<pthreahd.h>

#include<stsw_defs.h>
#include<stdint.h>


namespace stream_switch {

class ProtoClientHeartbeatReq;
typedef std::map<int, SourceApiHandlerEntry> SourceApiHanderMap;
struct ClientListType;
    
class StreamSource{
public:
    StreamSource();
    virtual ~StreamSource();
    
    // The following methods should be invoked by clients
    virtual int init(int argc, char* argv[]);
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
    // When    
    virtual void get_packet_statistic(uint64_t &total_packets, uint64_t &receive_packets);


    static int static_metadata_handlertypedef(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int static_key_frame_handler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int static_statistic_handler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int static_client_heartbeat_handler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    

protected:
    int metadata_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    int key_frame_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    int statistic_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    int client_heartbeat_handler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);

protected:
    zsock_t * api_socket;
    zsock_t * publish_socket;
    pthread_mutex_t lock;
    pthread_t * api_thread;
    SourceApiHanderMap api_handler_map;
    uint32_t flags; /*bit 0: meta data ready*/        
    void * staitistic;
        
    void * stream_meta;
    uint32_t cur_bps;
    int64_t last_frame_sec;
    int32_t last_frame_usec;
    int stream_state;
    ClientListType * clientList;
    
                         
};
}

#endif