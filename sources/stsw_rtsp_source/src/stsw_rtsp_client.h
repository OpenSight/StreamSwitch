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


// the LIve Rtsp Client listener class
//     An interface to handle live rtsp client's callback. 
// When some event happens, the rtsp client would 
// invokes the specific method of its listener in its 
// internal thread context. 
    
class LiveRtspClientListener{
public:    
    OnMediaFrame();
    OnError();
    OnMetaReady();     
};




class LiveRtspClient: public RTSPClient{
public:
    LiveRtspClient();
    ~LiveRtspClient();
    
    Init();
    Uninit();
    
    Start();
    Shutdown();
    
    GetMetadata();
    GetStatisticData();
    ResetStatistic();
    
    static void continueAfterOPTIONS(RTSPClient *client, int resultCode, char* resultString);  
    static void continueAfterDESCRIBE(RTSPClient *client, int resultCode, char* resultString);
    static void continueAfterSETUP(RTSPClient *client, int resultCode, char* resultString);
    static void continueAfterPLAY(RTSPClient *client, int resultCode, char* resultString);
    
};






typedef void (*RtspClientConstructCallback)(int resultCode, FramedSource * videoSource, FramedSource * audioSource);
typedef void (*RtspClientErrorCallback)(int resultCode);

int startRtspClient(char const* url, char const* progName, 
                    char const* userName, char const* passwd,
                    RtspClientConstructCallback constructCallback, 
                    RtspClientErrorCallback errorCallback);



void shutdownRtspClient(void);

int getQOSVideoData(unsigned  *outMeasurementTime,
                    unsigned  *outNumPacketsReceived,
                    unsigned  *outNumPacketsExpected, 
                    unsigned  *outKbitsPerSecondMin,
                    unsigned  *outKbitsPerSecondMax,
                    unsigned  *outKbitsPerSecondAvg,
                    unsigned  *outKbitsPerSecondNow,
                    unsigned  *outPacketLossPercentageMin,
                    unsigned  *outPacketLossPercentageMax,
                    unsigned  *outPacketLossPercentageAvg, 
                    unsigned  *outPacketLossPercentageNow,
                    unsigned  *outFpsAvg,
                    unsigned  *outFpsNow,
                    unsigned  *outGovAvg,
                    unsigned  *outGovNow
                    );

#define RELAY_CLIENT_RESULT_SUCCESSFUL 0 
#define RELAY_CLIENT_RESULT_CONNECT_FAIL -1 
#define RELAY_CLIENT_RESULT_DESCRIBE_ERR -2 
#define RELAY_CLIENT_RESULT_MEDIASESSION_CREATE_FAIL -3 
#define RELAY_CLIENT_RESULT_NO_SUBSESSION -4 
#define RELAY_CLIENT_RESULT_SUBSESSION_INIT_ERR -5 
#define RELAY_CLIENT_RESULT_SETUP_ERR -6 
#define RELAY_CLIENT_RESULT_PLAY_ERR -7 
#define RELAY_CLIENT_RESULT_BYE -8 
#define RELAY_CLIENT_RESULT_INTER_PACKET_GAP -9
#define RELAY_CLIENT_RESULT_SESSION_TIMER -10
#define RELAY_CLIENT_RESULT_USER_DEMAND -11
#define RELAY_CLIENT_RESULT_RESOUCE_ERR -12

#endif
