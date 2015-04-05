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
 * stsw_output_sink.cc
 *      MediaOutputSink class implementation file, 
 * contains all the methods and logic of the MediaOutputSink class
 * 
 * author: jamken
 * date: 2015-4-1
**/ 

#include "stsw_output_sink.h"


MediaOutputSink* MediaOutputSink::createNew(UsageEnvironment& env,
                                            MediaSubsession* subsession,                                          
                                            int32_t sub_stream_index,    
                                            size_t sink_buf_size)
{
    //sanity check
    if(subsession == NULL ||
       sub_stream_index < 0 ||
       sink_buf_size < 100){
        return NULL;
    }
    
    return new MediaOutputSink(env, subsession, 
                               sub_stream_index, sink_buf_size);
}


MediaOutputSink::MediaOutputSink(UsageEnvironment& env, 
                    MediaSubsession* subsession, 
                    int32_t sub_stream_index, size_t sink_buf_size)
: MediaSink(env), sink_buf_size_(sink_buf_size), subsession_(subsession), 
sub_stream_index_(sub_stream_index)
{
    recv_buf_ = new u_int8_t[sink_buf_size];
}


MediaOutputSink::~MediaOutputSink() {
    delete[] recv_buf_;
}

void MediaOutputSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) 
{
    MediaOutputSink* sink = (MediaOutputSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
// #define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void MediaOutputSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
  // We've just received a frame of data.  (Optionally) print out information about it:
#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
    envir() << "Stream index:\"" << sub_stream_index_ << "\"; ";
    envir() << subsession_->mediumName() << "/" << subsession_->codecName() << ":\tReceived " << frameSize << " bytes";
    if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
    char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
    sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
    envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
    if (subsession_->rtpSource() != NULL && !subsession_->rtpSource()->hasBeenSynchronizedUsingRTCP()) {
        envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
    }
#ifdef DEBUG_PRINT_NPT
    envir() << "\tNPT: " << subsession_->getNormalPlayTime(presentationTime);
#endif
    envir() << "\n";
#endif

    //update metadata if needed
    
    //check pts
    if(presentationTime.tv_sec < last_pts_.tv_sec ||
       (presentationTime.tv_sec == last_pts_.tv_sec &&
        presentationTime.tv_usec < last_pts_.tv_usec)){
        //the frame pts invalid, just drop the frame
        
        envir() << "A media frame with invalid timestamp from " 
                << "Stream index:\"" << sub_stream_index_ << "\" ("
                << subsession_->mediumName() << "/" << subsession_->codecName() ;
        char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
        sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
        envir() << " PTS: " << (int)presentationTime.tv_sec << "." << uSecsStr;
        if (subsession_->rtpSource() != NULL && !subsession_->rtpSource()->hasBeenSynchronizedUsingRTCP()) {
            envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
        }
        envir() <<")\n";
        
        goto out;
        
    }else{
        //update the last pts
        last_pts_.tv_sec = presentationTime.tv_sec;
        last_pts_.tv_usec = presentationTime.tv_usec;
    }
    
    
    //callback the parent rtsp client frame receive interface
    
    
out:    
  
    // Then continue, to request the next frame of data:
    continuePlaying();
}


Boolean MediaOutputSink::continuePlaying() {
    if (fSource == NULL) return False; // sanity check (should not happen)

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(recv_buf_, sink_buf_size_,
                        afterGettingFrame, this,
                        onSourceClosure, this);
    return True; 
}