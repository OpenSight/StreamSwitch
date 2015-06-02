/*
 * stsw_rtmp_source.h
 *
 *  Created on: Apr 12, 2015
 *      Author: hyt
 */

#ifndef STREAMSWITCH_SOURCES_STSW_RTMP_SOURCE_SRC_STSW_RTMP_SOURCE_H_
#define STREAMSWITCH_SOURCES_STSW_RTMP_SOURCE_SRC_STSW_RTMP_SOURCE_H_


#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>

#include <librtmp/rtmp.h>
#include <librtmp/log.h>
#include <stream_switch.h>



class RtmpClientSource: public stream_switch::SourceListener
{
public:
    static RtmpClientSource* Instance()
    {
        if ( NULL == s_instance )
        {
            s_instance = new RtmpClientSource();
        }

        return s_instance;
    }

    static void Uninstance()
    {
        if ( NULL != s_instance )
        {
            delete s_instance;
            s_instance = 0;
        }
    }

    void SetQuit();
    int Init(std::string stream_name, int source_tcp_port, int queue_size, int debug_flags);
    void Uninit();
    int Connect(std::string rtmpUrl);
    int HandlePacket(RTMPPacket& packet);
    int HandleonMetaData(char *body, unsigned int len);

    void OnKeyFrame(void) {};
    void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic) {};

private:
    RtmpClientSource();
    virtual ~RtmpClientSource();

    static RtmpClientSource * s_instance;

    char quit_;
    char metaReady_;
    stream_switch::StreamSource source_;
    std::string rtmpUrl_;
    RTMP*  rtmp_;

};



#endif /* STREAMSWITCH_SOURCES_STSW_RTMP_SOURCE_SRC_STSW_RTMP_SOURCE_H_ */
