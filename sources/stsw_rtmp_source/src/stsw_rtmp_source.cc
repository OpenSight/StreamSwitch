/*
 * stsw_rtmp_source.cc
 *
 *  Created on: Apr 12, 2015
 *      Author: hyt
 */

#include <string.h>

#include "stsw_rtmp_source.h"


extern stream_switch::RotateLogger * logger;


RtmpClientSource::RtmpClientSource(std::string rtmpUrl)
    :rtmp_(),
     source_()
{

    int len;

    len = rtmpUrl.length();
    if ((len > 512) || (len <= 7)) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Invalid RTMP URL %s, too long or too short", rtmpUrl);
        return;
    }
    rtmpUrl_ = rtmpUrl;
}


int RtmpClientSource::Connect() {

    RTMPPacket* packet;
    char url[512];

    if (rtmpUrl_.empty()) {
        return -1;
    }

    if ((rtmp_ = RTMP_Alloc()) == NULL) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Unable to alloc memory");
        return -1;
    }

    RTMP_Init(rtmp_);
    memcpy(url, rtmpUrl_.c_str(), rtmpUrl_.length());
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


    return 0;
}


RtmpClientSource::~RtmpClientSource() {

    if (rtmp_) {
        RTMP_Close(rtmp_);
        RTMP_Free(rtmp_);
    }
    rtmp_ = NULL;
}


