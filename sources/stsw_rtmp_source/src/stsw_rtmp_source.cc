/*
 * stsw_rtmp_source.cc
 *
 *  Created on: Apr 12, 2015
 *      Author: hyt
 */

#include <string.h>
#include <iostream>

#include "stsw_rtmp_source.h"


extern stream_switch::RotateLogger * logger;

RtmpClientSource* RtmpClientSource::s_instance = NULL;

RtmpClientSource::RtmpClientSource( )
    :quit_(0),
     source_(),
     rtmp_()
{
}


int RtmpClientSource::Connect(std::string rtmpUrl) {

    RTMPPacket packet = { 0 };
    char url[512];

    int len;

    len = rtmpUrl.length();
    if ((len > 512) || (len <= 7)) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Invalid RTMP URL %s, too long or too short", rtmpUrl);
        return -1;
    }
    rtmpUrl_ = rtmpUrl;

    if ((rtmp_ = RTMP_Alloc()) == NULL) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Unable to alloc memory");
        return -1;
    }

    RTMP_Init(rtmp_);
    memcpy(url, rtmpUrl_.c_str(), rtmpUrl_.length());
    url[rtmpUrl_.length()] = '\0';
    if (!RTMP_SetupURL(rtmp_, url)) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Failed to setup URL");
        return -1;
    }

    if (!RTMP_Connect(rtmp_, NULL)) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Failed to connect to RTMP server");
        return -1;
    }
    ROTATE_LOG(logger, stream_switch::LOG_LEVEL_INFO, "Connected to %s", rtmpUrl_.c_str());


    if (!RTMP_ConnectStream(rtmp_, 0)) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Failed to connect to stream");
        return -1;
    }
    ROTATE_LOG(logger, stream_switch::LOG_LEVEL_INFO, "Connected to stream");

    while(RTMP_ReadPacket(rtmp_, &packet) && not quit_) {
        if (RTMPPacket_IsReady(&packet))
        {
            ROTATE_LOG(logger, stream_switch::LOG_LEVEL_INFO, "received");
            if (!packet.m_nBodySize)
                continue;

            // we have one whole packet ready
            std::cout << (int)packet.m_packetType;

            RTMPPacket_Free(&packet);
        }
    }

    return 0;
}


RtmpClientSource::~RtmpClientSource() {

    if (rtmp_) {
        RTMP_Close(rtmp_);
        RTMP_Free(rtmp_);
    }
    rtmp_ = NULL;
}


void RtmpClientSource::SetQuit() {
    quit_ = 1;
}

