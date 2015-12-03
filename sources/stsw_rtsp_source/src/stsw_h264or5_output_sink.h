/**
 * This file is part of stsw_rtsp_source, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
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
 * stsw_h264or5_output_sink.h
 *      H264or5OutputSink class header file, declare all the interface of 
 *  H264or5OutputSink 
 * 
 * author: OpenSight Team
 * date: 2015-4-29
**/ 


#ifndef STSW_H264OR5_OUTPUT_SINK_H
#define STSW_H264OR5_OUTPUT_SINK_H

#ifndef STREAM_SWITCH
#define STREAM_SWITCH
#endif
#include "liveMedia.hh"
#include "stream_switch.h"
#include "stsw_output_sink.h"

#include<stdint.h>



////////////////////

// H264or5OutputSink class
//     this class is subclass of MediaOutputSink for H264 or H265, 
// It can analyze the H264 or H265 frames, and extract its VPS, SPS, PPS to update metadata
// and detect the frame type by h264or5't nal
// It's reponsieble for 
// 1) analyze the packet, and update the metadata of parent rtsp client. 
// 2) analyze the frame's type
class H264or5OutputSink: public MediaOutputSink {

public:
    static H264or5OutputSink* createNew(UsageEnvironment& env,
            LiveRtspClient *rtsp_client, 
            MediaSubsession* subsession, // identifies the kind of data 
                                         //that's being received
            int32_t sub_stream_index,    // identifies the stream itself 
            size_t sink_buf_size,        // receive buf size
            int h_number                 // 264 or 265
            );   
    


    H264or5OutputSink(UsageEnvironment& env, LiveRtspClient *rtsp_client,
                    MediaSubsession* subsession, 
                    int32_t sub_stream_index, size_t sink_buf_size, 
                    int h_number);
    // called only by "createNew()"
    virtual ~H264or5OutputSink();

    virtual void DoAfterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds);
    

    //utils
    virtual Boolean IsVCL(u_int8_t nal_unit_type);
    virtual Boolean IsIDR(u_int8_t nal_unit_type);    
    
protected:
    // redefined virtual functions:
    virtual Boolean continuePlaying();    

protected:

    int h_number_;
    u_int32_t recv_buf_size_;


    
};


#endif
