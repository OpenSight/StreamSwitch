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
 * stsw_stream_sink.h
 *      StreamSink class header file, declare all interfaces of StreamSink.
 * 
 * author: jamken
 * date: 2014-12-5
**/ 


#ifndef STSW_STREAM_SINK_H
#define STSW_STREAM_SINK_H
#include<map>
#include<stsw_defs.h>
#include<stdint.h>
#include<pthread.h>
#include<sys/time.h>

#define STSW_STREAM_RECEIVER_HEARTBEAT_INT  500  // the heartbeat interval for 
                                                 // stream receiver, in ms


namespace stream_switch {

class SinkListener; 
   
//opcode -> SinkSubHandlerEntry map
typedef std::map<int, SinkSubHandlerEntry> ReceiverSubHanderMap;

// the stream sink class
//     A stream sink class is used for a stream receiver application to receive the media 
// frames from one stream source
// Thread safety: 
//     most of methods(excepts for init/uninit) are thread safe, which means
// multi threads can invoke its methods on the same instance of this class 
// simultaneously without additional lock mechanism
    
class StreamSink{
public:
    StreamSink();
    virtual ~StreamSink();

    virtual int InitRemote(const std::string &source_ip, int source_tcp_port, 
                           const StreamClientInfo &client_info,
                           SinkListener *listener, 
                           uint32_t debug_flags,
                           std::string *err_info);    
    virtual int InitLocal(const std::string &stream_name, 
                          const StreamClientInfo &client_info,
                          SinkListener *listener, 
                          uint32_t debug_flags,
                          std::string *err_info);      
  
    virtual void Uninit();
    
    //flag check methods
    virtual bool IsInit();
    virtual bool IsStarted();  

   
    // Start up the receiver 
    // After source started up, the internal thread would be spawn up,  
    // then the source would begin to handle the incoming request and 
    // sent out its stream info message at intervals
    virtual int Start(std::string *err_info);
    
    // Stop the receiver
    // Stop the internal thread and wait for it.
    // After that, he source would no longer handle the incoming request nor 
    // sent out its stream info message 
    virtual void Stop();
    
    //the register/unregister function must call before start 
    virtual void RegisterSubHandler(int op_code, const char * channel_name, 
                                    SinkSubHandler handler, void * user_data);
    virtual void UnregisterSubHandler(int op_code);
    virtual void UnregisterAllSubHandler();      

    
    // accessors 
    virtual StreamClientInfo client_info();
    virtual void set_client_info(const StreamClientInfo &client_info);
    virtual StreamMetadata stream_meta();

    virtual uint32_t debug_flags(){
        return debug_flags_;
    }
    SinkListener * listener(){
        return listener_;
    }
    void set_listener(SinkListener *listener){
        listener_ = listener; 
    }
    
        
    virtual int UpdateStreamMetaData(int timeout, StreamMetadata * metadata, std::string *err_info);
    virtual int SourceStatistic(int timeout, MediaStatisticInfo * statistic, std::string *err_info);    
    virtual int KeyFrame(int timeout, std::string *err_info);
    virtual int ClientList(int timeout, uint32_t start_index, uint32_t request_num, 
                           uint32_t *  total_num, StreamClientList * client_list, 
                           std::string *err_info);
    
    virtual void ReceiverStatistic(MediaStatisticInfo * statistic);        
    
protected:

    //
    //internal used method

    static int StaticMediaFrameHandler(void * user_data, const ProtoCommonPacket * msg);
    virtual int MediaFrameHandler(const ProtoCommonPacket * msg);


    virtual int InitBase(const StreamClientInfo &client_info, 
                         SinkListener *listener,
                         uint32_t debug_flags, std::string *err_info);   

    virtual int SendRpcRequest(ProtoCommonPacket * request, int timeout, ProtoCommonPacket * reply,  std::string *err_info);    

    virtual int Heartbeat(int64_t now);
    
    virtual int SubscriberHandler();
    
    virtual void ClientHeartbeatHandler(int64_t now);
    
    static void * StaticThreadRoutine(void *);
    virtual void InternalRoutine();
    
    pthread_mutex_t& lock(){
        return lock_;
    }
    
    virtual uint32_t GetNextSeq();
    
   
    
private:
    std::string api_addr_;
    std::string subscriber_addr_;
    SocketHandle last_api_socket_;      //cache the last used api request sokcet
    SocketHandle client_hearbeat_socket_;
    SocketHandle subscriber_socket_;
    pthread_mutex_t lock_;
    pthread_t worker_thread_id_;
    uint32_t next_seq_;
    uint32_t debug_flags_;
    
    ReceiverSubHanderMap subsriber_handler_map_;
    
    SubStreamMediaStatisticVector statistic_;  
    
// stream source flags
#define STREAM_RECEIVER_FLAG_INIT 1
#define STREAM_RECEIVER_FLAG_STARTED 4
    uint32_t flags_;      

    StreamMetadata stream_meta_;

    int64_t last_heartbeat_time_;     // in milli-sec

    int64_t last_send_client_heartbeat_msec_;    
    int64_t next_send_client_heartbeat_msec_;
    
    StreamClientInfo client_info_;
    
    SinkListener *listener_;
                             
};

}

#endif