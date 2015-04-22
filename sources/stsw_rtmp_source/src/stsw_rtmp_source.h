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
#include <stream_switch.h>



class RtmpClientSource: public stream_switch::SourceListener{

public:
    RtmpClientSource(const char* rtmpUrl);
    ~RtmpClientSource();

    int Connect();

    void OnKeyFrame(void) {};
    void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic) {};


private:
    stream_switch::StreamSource source_;
    char  rtmpUrl_[512];
    RTMP*  rtmp_;

};



#endif /* STREAMSWITCH_SOURCES_STSW_RTMP_SOURCE_SRC_STSW_RTMP_SOURCE_H_ */
