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
 * stsw_rtsp_client.h
 *      LiveRtspClient class header file, Declare all the interface of 
 * LiveRtspClient
 * 
 * author: jamken
 * date: 2015-4-1
**/ 


#ifndef STSW_RTSP_CLIENT_H
#define STSW_RTSP_CLIENT_H

#include "liveMedia.hh"
#include "FramedSource.hh"
#include "stream_switch.h"

// the LIve Rtsp Client listener class
//     An interface to handle live rtsp client's callback. 
// When some event happens, the rtsp client would 
// invokes the specific method of its listener in its 
// internal thread context. 
    
    
enum RtspClientErrCode{
    RTSP_CLIENT_ERR_SUCCESSFUL = 0, 
    RTSP_CLIENT_ERR_CONNECT_FAIL = -1, 
    RTSP_CLIENT_ERR_DESCRIBE_ERR = -2,  
    RTSP_CLIENT_ERR_SETUP_ERR = -3,  
    RTSP_CLIENT_ERR_PLAY_ERR = -4,  
    RTSP_CLIENT_ERR_MEDIASESSION_CREATE_FAIL = -5,  
    RTSP_CLIENT_ERR_NO_SUBSESSION = -6,  
    RTSP_CLIENT_ERR_SUBSESSION_INIT_ERR = -7,  
    RTSP_CLIENT_ERR_SUBSESSION_BYE = -8,  
    RTSP_CLIENT_ERR_INTER_FRAME_GAP = -9, 
    RTSP_CLIENT_ERR_SESSION_TIMER = -10, 
    RTSP_CLIENT_ERR_USER_DEMAND = -11,
    RTSP_CLIENT_ERR_RESOUCE_ERR = -12,
    RTSP_CLIENT_ERR_TIME_ERR = -13,    
};

class MediaOutputSink;   
class PtsSessionNormalizer;
typedef std::vector<MediaOutputSink *> MediaOutputSinkList;

class LiveRtspClientListener{
public:    
    // When the client get a frame from remote server successfully, 
    // OnMediaFrame() would be invoke 
    virtual void OnMediaFrame(
        const stream_switch::MediaFrameInfo &frame_info, 
        const char * frame_data, 
        size_t frame_size
    );
    
    // When the client detect some error, 
    // OnError() would be invoked, normally, user would 
    // exit the program. user shouldn't call shutdown() method in 
    // this cb which would wait for the client's internal thread
    // exit so that would result into deadlock
    // After this cb is invoke, the rtsp client
    virtual void OnError(RtspClientErrCode err_code, const char * err_info);
    
    // when this client's metadata become ready, 
    // OnMetaReady() would be invoked. Normally, user would 
    // get the meta data and start streamswitch source in this callbak
    virtual void OnMetaReady(const stream_switch::StreamMetadata &metadata);  

    // when this client finish the rtsp process successfully, 
    // OnRtspOK() would be invoked. Normally, user would config the source
    // status in this callback.
    virtual void OnRtspOK();  
};




class LiveRtspClient: public RTSPClient{
public:
    static LiveRtspClient * CreateNew(UsageEnvironment& env, char const* rtspURL,                    
			       Boolean streamUsingTCP = True, Boolean enableRtspKeepAlive = True, 
                   char const* singleMedium = NULL, 
                   char const* userName = NULL, char const* passwd = NULL, 
                   LiveRtspClientListener * listener = NULL, 
                   int verbosityLevel = 0);

    LiveRtspClient(UsageEnvironment& env, char const* rtspURL, 
			       Boolean streamUsingTCP, Boolean enableRtspKeepAlive, 
                   char const* singleMedium, 
                   char const* userName, char const* passwd,  
                   LiveRtspClientListener * listener, 
                   int verbosityLevel);
                   
    virtual ~LiveRtspClient();
    
    
    virtual int Start();
    virtual void Shutdown();
    

    virtual bool IsMetaReady()
    {
        return (is_metadata_ok_ == True);
    }
    virtual stream_switch::StreamMetadata *mutable_metadata()
    {
        return &metadata_;
    }
    virtual bool CheckMetadata();
    
    //TODO
    //virtual int GetStatisticData();
    //virtual void ResetStatistic();
    
    virtual void AfterGettingFrame(int32_t sub_stream_index, 
                           stream_switch::MediaFrameType frame_type, 
                           struct timeval timestamp, 
                           unsigned frame_size, 
                           const char * frame_buf);
                           
    virtual void SetListener(LiveRtspClientListener * listener)
    {
        listener_ = listener;
    }
    
protected:
    ////////////////////////////////////////////////////////////
    //RTSP callback function
    static void ContinueAfterOPTIONS(RTSPClient *client, int resultCode, char* resultString);  
    static void ContinueAfterDESCRIBE(RTSPClient *client, int resultCode, char* resultString);
    static void ContinueAfterSETUP(RTSPClient *client, int resultCode, char* resultString);
    static void ContinueAfterPLAY(RTSPClient *client, int resultCode, char* resultString);
    static void ContinueAfterTEARDOWN(RTSPClient* client, int resultCode, char* resultString);
    static void ContinueAfterKeepAlive(RTSPClient* client, int resultCode, char* resultString);
    static void SubsessionAfterPlaying(void* clientData);
    
    
    //////////////////////////////////////////////////
    //Timer task handler
    static void RtspKeepAliveHandler(void* clientData);
    static void SessionTimerHandler(void* clientData);
    static void SubsessionByeHandler(void* clientData);
    static void RtspClientConnectTimeout(void* clientData);
    static void CheckInterFrameGaps(void* clientData);    
    
    
    

    

    ////////////////////////////////////////////////////////////
    //RTSP command sending functions
    virtual void GetOptions(RTSPClient::responseHandler* afterFunc);
    
    virtual void GetSDPDescription(RTSPClient::responseHandler* afterFunc);

    virtual void SetupSubsession(MediaSubsession* subsession, 
        Boolean streamUsingTCP, 
        Boolean forceMulticastOnUnspecified, 
        RTSPClient::responseHandler* afterFunc); 

    virtual void StartPlayingSession(MediaSession* session, 
        double start, double end, 
        float scale, RTSPClient::responseHandler* afterFunc); 

    virtual void StartPlayingSession(MediaSession* session, 
        char const* absStartTime, char const* absEndTime, float scale, 
        RTSPClient::responseHandler* afterFunc);

    virtual void TearDownSession(MediaSession* session, 
        RTSPClient::responseHandler* afterFunc);
        
    virtual void KeepAliveSession(MediaSession* session, 
        RTSPClient::responseHandler* afterFunc);
        
        
    /////////////////////////////////////////////////////////
    //error handler        
    virtual void HandleError(RtspClientErrCode err_code, 
        const char * err_info);  
        
    
    ///////////////////////////////////////////////////////////
    //utils
    
    virtual void SetUserAgentString(char const* userAgentString);
    
    virtual void closeMediaSinks();
    
    virtual void SetupStreams();
    virtual int SetupSinks();
    
    virtual void SetupMetaFromSession();
    
protected: 
    LiveRtspClientListener * listener_;
    Boolean are_already_shutting_down_;
    Boolean stream_using_tcp_;
    Boolean enable_rtsp_keep_alive_; 
    char * single_medium_;   
    std::string rtsp_url_;
    Authenticator * our_authenticator;
    MediaSession* session_;
    uint32_t ssrc_;
    stream_switch::StreamMetadata metadata_;
    Boolean is_metadata_ok_;
    

    TaskToken session_timer_task_;
    TaskToken inter_frame_gap_check_timer_task_;
    TaskToken rtsp_keep_alive_task_;
    TaskToken rtsp_timeout_task_;
    
    Boolean has_receive_keep_alive_response;
    double duration_;
    double endTime_;
    
    struct timeval client_start_time_;
    struct timeval last_frame_time_;
    
    PtsSessionNormalizer *pts_session_normalizer;
    
    
    //internal used in setup streams
    Boolean made_progress_;
    MediaSubsessionIterator* setup_iter_;
    MediaSubsession *cur_setup_subsession_;
    
};





#endif
