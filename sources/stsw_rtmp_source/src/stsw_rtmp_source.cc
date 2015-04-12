/*
 * stsw_rtmp_source.cc
 *
 *  Created on: Apr 12, 2015
 *      Author: hyt
 */

#include "stsw_rtmp_source.h"


RtmpClientSource::RtmpClientSource() {

    RTMPPacket* packet;

    rtmp_ = RTMP_Alloc();

    RTMP_Init(rtmp_);

    RTMP_SetupURL(rtmp_, "rtmp://192.168.1.180:1935/vod/wow.flv");

    RTMP_ReadPacket(rtmp_, packet);



}


RtmpClientSource::~RtmpClientSource() {

    RTMP_Close(rtmp_);
    RTMP_Free(rtmp_);
    rtmp_ = NULL;
}


int main() {



    return 0;
}
