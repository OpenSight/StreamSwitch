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
#include<pthread.h>
#include<sys/time.h>

#define STSW_STREAM_SOURCE_HEARTBEAT_INT  1000  // the heartbeat interval for 
                                                // stream source, in ms


namespace stream_switch {

class ProtoClientHeartbeatReq;
typedef std::map<int, SourceApiHandlerEntry> SourceApiHanderMap;
struct ReceiversInfoType;
typedef void * SocketHandle;


// the Source class
//     A source class is used for a stream source application to emit its media 
// frames to multi stream receiver
// Thread safety: 
//     most of methods(excepts for init/uninit) are thread safe which means
// multi threads can invoke its methods on the same instance of this class 
// simultaneously without additional lock mechanism
    
class StreamSource{
public:
    StreamSource();
    virtual ~StreamSource();
    
    // The following methods should be invoked by clients
    
    // init this source, note that it's not thread-safe
    // Args:
    //     stream_name string in: the stream name of this source, 
    //         which is used to bind the api socket to unix domain address
    //     tcp_port int in: the tcp port of this source, api socket would listen
    //         on this port, and publish socket would listen on tcp_port + 1. If
    //         this param is 0, means this source never listen on tcp
    //     errInfo string out: error tips if failed
    //
    // return:
    //     0 if successful, or -1 if error;
    virtual int Init(const std::string &stream_name, int tcp_port, std::string *err_info);
    
    // un-init the source, note that it's not thread-safe
    virtual void Uninit();
    
    //flag check methods
    virtual bool IsInit();
    virtual bool IsMetaReady();
    virtual bool IsStarted();
    

   
    // Start up the source 
    // After source started up, the internal thread would be spawn up,  
    // then the source would begin to handle the incoming request and 
    // sent out its stream info message at intervals
    virtual int Start(std::string *err_info);
    
    // Stop the source
    // Stop the internal thread and wait for it.
    // After that, he source would no longer handle the incoming request nor 
    // sent out its stream info message 
    virtual void Stop();
    
    // send out a media frame
    // send out a media frame of a specific sub stream with a valid sequence number
    // Args:
    //     sub_stream_index int32_t in: the sub stream index in the configured 
    //         metadata of the source
    //     frame_seq uint64_t in: the seq of this frame in sub stream, it should be 
    //         start from 1, and increased by 1 for each next frame in the sub stream.
    //         if the seq skips, the source would consider frame loss. If your source 
    //         stream cannot provide frame seq calculation, this param should be set
    //         to 0 forever.
    //     frame_type MediaFrameType in: the frame type 
    //     timestamp timeval in: the pts for the frame
    //     ssrc uint32_t in: must match the ssrc in the metadata of source
    //     data string in: the data in the frame
    virtual int SendMediaData(int32_t sub_stream_index, 
                              uint64_t frame_seq,     
                              MediaFrameType frame_type,
                              const struct timeval &timestamp, 
                              uint32_t ssrc, 
                              std::string data, 
                              std::string *err_info);
    
    
    // accessors 
    void set_stream_state(int stream_state);
    int stream_state();
    void set_stream_meta(const StreamMetadata & stream_meta);
    StreamMetadata stream_meta();
    pthread_mutex_t& lock(){
        return lock_;
    }
    

   
    virtual void RegisterApiHandler(int op_code, SourceApiHandler handler, void * user_data);
    virtual void UnregisterApiHandler(int op_code);
    virtual void UnregisterAllApiHandler();    
    
        
    // the following methods need client to override
    // to fulfill its functions. They would be invoked
    // by the internal api thread 
        
    // key_frame
    // When the receiver request a key frame, it would be 
    // invoked
    virtual void KeyFrame(void);
        

protected:

    static int StaticMetadataHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int StaticKeyFrameHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int StaticStatisticHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    static int StaticClientHeartbeatHandler(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    
    virtual int MetadataHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    virtual int KeyFrameHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    virtual int StatisticHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);
    virtual int ClientHeartbeatHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);

    virtual int RpcHandler();
    virtual int Heartbeat(int64_t now);
    
    static void * ThreadRoutine(void *);
    
    
    virtual void SendStreamInfo(void);
    
    
    // send the msg from the publish socket on the given channel
    // 
    void SendPublishMsg(char * channel_name, const ProtoCommonPacket &msg);
    
    
private:
    std::string stream_name_;
    int tcp_port_;
    SocketHandle api_socket_;
    SocketHandle publish_socket_;
    pthread_mutex_t lock_;
    pthread_t api_thread_id_;
    bool thread_end_;
    SourceApiHanderMap api_handler_map_;
    
// stream source flags
#define STREAM_SOURCE_FLAG_INIT 1
#define STREAM_SOURCE_FLAG_META_READY 2
#define STREAM_SOURCE_FLAG_STARTED 4
    uint32_t flags_;      

    SubStreamMediaStatisticVector statistic_;
    StreamMetadata stream_meta_;
    uint32_t cur_bytes;
    uint32_t cur_bps_;
    int64_t last_frame_sec_;
    int32_t last_frame_usec_;
    int stream_state_;
    ReceiversInfoType * receivers_info_;
    int64_t last_heartbeat_time_;     // in milli-sec
                             
};

}

#endif