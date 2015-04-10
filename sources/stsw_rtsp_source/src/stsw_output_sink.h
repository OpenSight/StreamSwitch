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
 * stsw_output_sink.h
 *      MediaOutputSink class header file, declare all the interface of 
 *  MediaOutputSink 
 * 
 * author: jamken
 * date: 2015-4-3
**/ 


#ifndef STSW_OUTPUT_SINK_H
#define STSW_OUTPUT_SINK_H

#ifndef STREAM_SWITCH
#define STREAM_SWITCH
#endif
#include "liveMedia.hh"

#include<stdint.h>

////////// PtsSessionNormalizer and PtsSubsessionNormalizer definitions //////////

// the Media Output Sink class
//     this class is used to get the media frame from the underlayer source
// perform some intermediate processing, and output them to the parent rtsp 
// client.
// It's reponsieble for 
// 1) check the PTS is monotonic, otherwise drop the frame
// 2) analyze the packet, and update the metadata of parent rtsp client. 
// 3) analyze the frame's type
class MediaOutputSink: public MediaSink {

public:
    static MediaOutputSink* createNew(UsageEnvironment& env,
            MediaSubsession* subsession, // identifies the kind of data 
                                         //that's being received
            int32_t sub_stream_index,    // identifies the stream itself 
            size_t sink_buf_size        // receive buf size
            );   
    


    MediaOutputSink(UsageEnvironment& env, MediaSubsession* subsession, 
                  int32_t sub_stream_index, size_t sink_buf_size);
    // called only by "createNew()"
    virtual ~MediaOutputSink();
    

    
    

    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds);

    virtual void DoAfterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds);

private:
    // redefined virtual functions:
    virtual Boolean continuePlaying();

protected:
    u_int8_t* recv_buf_;
    size_t sink_buf_size_;
    MediaSubsession* subsession_;
    int32_t sub_stream_index_;  
    struct timeval last_pts_; 
    
};


#endif
