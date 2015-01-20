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

class SourceListener;


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
    //     listener SourceListener * in: the listener of this source
    //     debug_flags uint32_t in: the debug flags of this source
    //     errInfo string out: error tips if failed
    //
    // return:
    //     0 if successful, or -1 if error;
    virtual int Init(const std::string &stream_name, int tcp_port, 
                     SourceListener *listener, 
                     uint32_t debug_flags, std::string *err_info);
    
    // un-init the source, note that it's not thread-safe
    virtual void Uninit();
    
    //flag check methods
    virtual bool IsInit();
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
    
    // send out a live media frame
    // send out a llive media frame of a specific sub stream with a valid sequence number.
    // replay source should not invoke this method.
    // Args:
    //     media_frame MediaDataFrame in : the media frame to send
    //     err_info string out: the error info if failed
    virtual int SendLiveMediaFrame(const MediaDataFrame &media_frame, 
                              std::string *err_info);
    
    
    // accessors 
    void set_stream_state(int stream_state);
    int stream_state();
    void set_stream_meta(const StreamMetadata & stream_meta);
    StreamMetadata stream_meta();

    uint32_t debug_flags(){
        return debug_flags_;
    }   

    SourceListener * listener(){
        return listener_;
    }
    void set_listener(SourceListener *listener){
        listener_ = listener; 
    }

   
    virtual void RegisterApiHandler(int op_code, SourceApiHandler handler, void * user_data);
    virtual void UnregisterApiHandler(int op_code);
    virtual void UnregisterAllApiHandler();  

protected:

    static int StaticMetadataHandler(void * user_data, ProtoCommonPacket * request, ProtoCommonPacket * reply);
    static int StaticKeyFrameHandler(void * user_data, ProtoCommonPacket * request, ProtoCommonPacket * reply);
    static int StaticStatisticHandler(void * user_data, ProtoCommonPacket * request, ProtoCommonPacket * reply);
    static int StaticClientHeartbeatHandler(void * user_data, ProtoCommonPacket * request, ProtoCommonPacket * reply);
    
    virtual int MetadataHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply);
    virtual int KeyFrameHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply);
    virtual int StatisticHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply);
    virtual int ClientHeartbeatHandler(ProtoCommonPacket * request, ProtoCommonPacket * reply);

    virtual int RpcHandler();
    virtual int Heartbeat(int64_t now);
    
    static void * StaticThreadRoutine(void *arg);
    virtual void InternalRoutine();    
    
    virtual void SendStreamInfo(void);    
    
    // send the msg from the publish socket on the given channel
    // 
    virtual void SendPublishMsg(char * channel_name, const ProtoCommonPacket &msg);
    
    
    pthread_mutex_t& lock(){
        return lock_;
    }
    
private:
    std::string stream_name_;
    int tcp_port_;
    SocketHandle api_socket_;
    SocketHandle publish_socket_;
    pthread_mutex_t lock_;
    pthread_t api_thread_id_;
    SourceApiHanderMap api_handler_map_;
    uint32_t debug_flags_;
    
// stream source flags
#define STREAM_SOURCE_FLAG_INIT 1
#define STREAM_SOURCE_FLAG_STARTED 4
    uint32_t flags_;      

    SubStreamMediaStatisticVector statistic_;
    StreamMetadata stream_meta_;
    uint32_t cur_bytes_;
    uint32_t cur_bps_;
    int64_t last_frame_sec_;
    int32_t last_frame_usec_;
    int stream_state_;
    ReceiversInfoType * receivers_info_;
    int64_t last_heartbeat_time_;     // in milli-sec
    
    SourceListener *listener_;
};

}

#endif