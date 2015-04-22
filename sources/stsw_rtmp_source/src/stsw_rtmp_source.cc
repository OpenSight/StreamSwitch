/*
 * stsw_rtmp_source.cc
 *
 *  Created on: Apr 12, 2015
 *      Author: hyt
 */

#include <string.h>

#include "stsw_rtmp_source.h"


RtmpClientSource::RtmpClientSource(const char* rtmpUrl)
    :rtmp_(NULL),
     source_()
{

    int len;

    len = strlen(rtmpUrl);
    if ((len > 512) || (len <= 7)) {
        rtmpUrl_[0] = 0;
        return;
    }
    memcpy(rtmpUrl_, rtmpUrl, len);
    rtmpUrl_[len] = 0;
}


int RtmpClientSource::Connect() {

    RTMPPacket* packet;

    if (strlen(rtmpUrl_) == 0) {
        return -1;
    }

    if ((rtmp_ = RTMP_Alloc()) == NULL) {
        return -1;
    }

    RTMP_Init(rtmp_);
    if (!RTMP_SetupURL(rtmp_, rtmpUrl_)) {
        return -1;
    }

    if (!RTMP_Connect(rtmp_, NULL)) {
        return -1;
    }

    if (!RTMP_ConnectStream(rtmp_, 0)) {
        return -1;
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


