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
 * stsw_mpeg4_output_sink.h
 *      Mpeg4OutputSink class header file, declare all the interface of 
 *  Mpeg4OutputSink 
 * 
 * author: jamken
 * date: 2015-4-29
**/ 


#ifndef STSW_MPEG4_OUTPUT_SINK_H
#define STSW_MPEG4_OUTPUT_SINK_H

#ifndef STREAM_SWITCH
#define STREAM_SWITCH
#endif
#include "liveMedia.hh"
#include "stream_switch.h"
#include "stsw_output_sink.h"

#include<stdint.h>



////////////////////

// Mpeg4OutputSink class
//     this class is subclass of MediaOutputSink for MPEG4 video, 
// It can analyze the MPEG4 frames, and extract its VS, VO, VOL header to update metadata
// and detect the frame type by MPEG4 header
// It's reponsieble for 
// 1) analyze the packet, and update the metadata of parent rtsp client. 
// 2) analyze the frame's type
class Mpeg4OutputSink: public MediaOutputSink {

public:
    static Mpeg4OutputSink* createNew(UsageEnvironment& env,
            LiveRtspClient *rtsp_client, 
            MediaSubsession* subsession, // identifies the kind of data 
                                         //that's being received
            int32_t sub_stream_index,    // identifies the stream itself 
            size_t sink_buf_size        // receive buf size
            );   
    


    Mpeg4OutputSink(UsageEnvironment& env, LiveRtspClient *rtsp_client,
                    MediaSubsession* subsession, 
                    int32_t sub_stream_index, size_t sink_buf_size);
    // called only by "createNew()"
    virtual ~Mpeg4OutputSink();

    virtual void DoAfterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds);
    
    Boolean IsKeyFrame(unsigned char * buf, unsigned buf_size);
    Boolean IsIncludeVOP(unsigned char * buf, unsigned buf_size);    
    
};


#endif
