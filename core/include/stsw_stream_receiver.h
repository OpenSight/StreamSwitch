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
 * stsw_stream_receiver.h
 *      StreamReceiver class header file, declare all interfaces of StreamReceiver.
 * 
 * author: jamken
 * date: 2014-12-5
**/ 


#ifndef STSW_STREAM_RECEIVER_H
#define STSW_STREAM_RECEIVER_H
#include<map>
#include<stsw_defs.h>
#include<stdint.h>
#include<pthread.h>
#include<sys/time.h>

#define STSW_STREAM_RECEIVER_HEARTBEAT_INT  1000  // the heartbeat interval for 
                                                 // stream receiver, in ms


namespace stream_switch {

class ProtoClientHeartbeatReq;
typedef std::map<int, SourceApiHandlerEntry> SourceApiHanderMap;
struct ReceiversInfoType;
typedef void * SocketHandle;


// the Receiver class
//     A Receiver class is used for a stream receiver application to receive the media 
// frames from one stream source
// Thread safety: 
//     most of methods(excepts for init/uninit) are thread safe, which means
// multi threads can invoke its methods on the same instance of this class 
// simultaneously without additional lock mechanism
    
class StreamReceiver{
public:
    StreamReceiver();
    virtual ~StreamReceiver();

    virtual int InitRemote(const std::string &source_ip, int source_tcp_port, std::string *err_info);    
    virtual int InitLocal(const std::string &source_ip, int source_tcp_port, std::string *err_info);      
  
    virtual void Uninit();
    
    //flag check methods
    virtual bool IsInit();
    virtual bool IsMetaReady();
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

    
    // accessors 
    pthread_mutex_t& lock(){
        return lock_;
    }
    


        

protected:
    virtual int InitBase(std::string *err_info);   


    virtual int Heartbeat(int64_t now);
    
    static void * ThreadRoutine(void *);
    
    
    virtual void SendStreamInfo(void);
    

    
    
private:
    std::string api_addr;
    std::string publish_addr;
    SocketHandle api_socket_;    
    SocketHandle client_hearbeat_socket_;
    SocketHandle subscriber_socket_;
    pthread_mutex_t lock_;
    pthread_t worker_thread_id_;
    uint32_t ssrc;
    
    
// stream source flags
#define STREAM_RECEIVER_FLAG_INIT 1
#define STREAM_RECEIVER_FLAG_META_READY 2
#define STREAM_RECEIVER_FLAG_STARTED 4
    uint32_t flags_;      

    StreamMetadata stream_meta_;



    int64_t last_heartbeat_time_;     // in milli-sec
                             
};

}

#endif