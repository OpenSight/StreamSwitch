/*
 * stsw_rtmp_source.cc
 *
 *  Created on: Apr 12, 2015
 *      Author: hyt
 */

#include <string.h>
#include <iostream>

#include "stsw_rtmp_source.h"
#include <librtmp/amf.h>


extern stream_switch::RotateLogger * logger;

RtmpClientSource* RtmpClientSource::s_instance = NULL;

#define SAVC(x) static const AVal av_##x = AVC(#x)


RtmpClientSource::RtmpClientSource( )
    :quit_(0),
     metaReady_(0),
     source_(),
     rtmp_()
{
}


int RtmpClientSource::Init(
        std::string stream_name,
        int source_tcp_port,
        int queue_size,
        int debug_flags)
{
    int ret;
    std::string err_info;

    //init source
    ret = source_.Init(stream_name, source_tcp_port,
                       queue_size,
                       this, debug_flags, &err_info);
    if (ret) {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "Failed to init stream source");
        return ret;
    }




}


void RtmpClientSource::Uninit() {

    source_.Uninit();
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
            if (!packet.m_nBodySize)
                continue;

            ROTATE_LOG(logger, stream_switch::LOG_LEVEL_DEBUG, "Message type %x received", packet.m_packetType);
            HandlePacket(packet);
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


int RtmpClientSource::HandlePacket(RTMPPacket& packet) {


    switch (packet.m_packetType) {
    case RTMP_PACKET_TYPE_INFO:
        /* metadata (notify) */
        if (HandleonMetaData(packet.m_body, packet.m_nBodySize) != 0) {
            return -1;
        }
        break;
    }

    return 0;
}

SAVC(onMetaData);
SAVC(duration);

int RtmpClientSource::HandleonMetaData(char *body, unsigned int len) {

    AMFObject obj;
    AVal metastring;
    int ret = FALSE;

    int nRes = AMF_Decode(&obj, body, len, FALSE);
    if (nRes < 0)
    {
        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_ERR, "error decoding meta data message");
        return -1;
    }

    AMF_Dump(&obj);
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &metastring);

    if (AVMATCH(&metastring, &av_onMetaData))
    {
        stream_switch::StreamMetadata metadata;
        stream_switch::SubStreamMetadata sub_metadata;
        AMFObjectProperty prop;

        ROTATE_LOG(logger, stream_switch::LOG_LEVEL_INFO, "onMetaData Message received");
        if (RTMP_FindFirstMatchingProperty(&obj, &av_duration, &prop))
        {
          r->m_fDuration = prop.p_vu.p_number;
          /*RTMP_Log(RTMP_LOGDEBUG, "Set duration: %.2f", m_fDuration); */
        }
        /* Search for audio or video tags */
        if (RTMP_FindPrefixProperty(&obj, &av_video, &prop))
            r->m_read.dataType |= 1;
        if (RTMP_FindPrefixProperty(&obj, &av_audio, &prop))
            r->m_read.dataType |= 4;


        source_.set_stream_meta(metadata);
    }
    AMF_Reset(&obj);


    metaReady_ = 1;
    return 0;
}


void RtmpClientSource::SetQuit() {
    quit_ = 1;
}

