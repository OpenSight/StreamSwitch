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
 * stsw_output_sink.cc
 *      MediaOutputSink class implementation file, 
 * contains all the methods and logic of the MediaOutputSink class
 * 
 * author: OpenSight Team
 * date: 2015-4-1
**/ 

#include "stsw_output_sink.h"
#include "stsw_rtsp_client.h"

MediaOutputSink* MediaOutputSink::createNew(UsageEnvironment& env,
                                            LiveRtspClient *rtsp_client,
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
    
    return new MediaOutputSink(env, rtsp_client, subsession, 
                               sub_stream_index, sink_buf_size);
}


MediaOutputSink::MediaOutputSink(UsageEnvironment& env, 
                    LiveRtspClient *rtsp_client,
                    MediaSubsession* subsession, 
                    int32_t sub_stream_index, size_t sink_buf_size)
: MediaSink(env), sink_buf_size_(sink_buf_size), subsession_(subsession), 
sub_stream_index_(sub_stream_index), rtsp_client_(rtsp_client), 
last_gap_(0.0), lost_frames_(0)
{
    recv_buf_ = new u_int8_t[sink_buf_size];
    last_pts_.tv_sec = 0;
    last_pts_.tv_usec = 0;
    last_data_frame_pts_.tv_sec = 0;
    last_data_frame_pts_.tv_usec = 0;
}


MediaOutputSink::~MediaOutputSink() {
    delete[] recv_buf_;    
    ClearQueue();
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
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
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

    

    
    
    DoAfterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);


  
out:    
  
    // Then continue, to request the next frame of data:
    continuePlaying();
}

void MediaOutputSink::DoAfterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds)
{


    //by default, pts require monotone increasing
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
        
        return;
        
    }else{
        //update the last pts
        last_pts_.tv_sec = presentationTime.tv_sec;
        last_pts_.tv_usec = presentationTime.tv_usec;
    }
    
    if(numTruncatedBytes > 0){
        envir() << "A media frame is truncated with bytes:" 
                << numTruncatedBytes << " from " 
                << "Stream index:\"" << sub_stream_index_ << "\" ("
                << subsession_->mediumName() << "/" << subsession_->codecName() 
                <<"), Dropped\n";   
        return;
    }
    
    
    //update metadata if needed    
    
    //analyze the frame's type
    stream_switch::MediaFrameType frame_type = 
        stream_switch::MEDIA_FRAME_TYPE_KEY_FRAME; //default is key data frame
    
    //callback the parent rtsp client frame receive interface
    if(rtsp_client_ != NULL){
        rtsp_client_->AfterGettingFrame(sub_stream_index_, frame_type, 
                                        presentationTime, frameSize, (char *)recv_buf_);
    }
    
        
}


void  MediaOutputSink::CheckLostByTime(stream_switch::MediaFrameType frame_type, 
                                   struct timeval presentationTime)
{
    if(frame_type == stream_switch::MEDIA_FRAME_TYPE_KEY_FRAME ||
       frame_type == stream_switch::MEDIA_FRAME_TYPE_DATA_FRAME){
        if(last_data_frame_pts_.tv_sec != 0 ||
           last_data_frame_pts_.tv_usec != 0){
            double last_pts_f = last_data_frame_pts_.tv_sec + 
                last_data_frame_pts_.tv_usec/1000000.0;
            double now_pts_f = presentationTime.tv_sec + 
                presentationTime.tv_usec/1000000.0;
            double cur_gap = now_pts_f - last_pts_f;
            
            //envir() << "cur_gap :" << cur_gap << "\n"; 
            
            if(last_gap_ > 0.001 && 
               cur_gap >= last_gap_ * 1.7){
               double lost = (int)((cur_gap - last_gap_) / last_gap_);
               lost_frames_ += (int)(lost + 0.5);
               //envir() << "lost_frames_ :" << (int)lost_frames_ << "\n"; 
               if(rtsp_client_!= NULL){
                   rtsp_client_->UpdateLostFrame(sub_stream_index_, lost_frames_);
               }
               
            }else{
               
               last_gap_ = cur_gap; 
               
            }
            
        }        
        
        last_data_frame_pts_ = presentationTime;
    }
}


//framebuf queue operations
void MediaOutputSink::PushOneFrame(stream_switch::MediaFrameType frame_type, 
                  struct timeval timestamp, 
                  unsigned frame_size, 
                  const char * frame_buf)
{
    FrameBuf temp_frame;
    temp_frame.frame_type = frame_type;
    temp_frame.presentation_time = timestamp;
    temp_frame.frame_size = frame_size;
    temp_frame.buf = new char[frame_size];
    memcpy(temp_frame.buf, frame_buf, frame_size);
    frame_queue_.push_back(temp_frame);
}



void MediaOutputSink::FlushQueue()
{
    FrameQueue::iterator it;
    for(it= frame_queue_.begin(); it!= frame_queue_.end(); it++){

        //callback the parent rtsp client frame receive interface
        if(rtsp_client_ != NULL){
            rtsp_client_->AfterGettingFrame(
                sub_stream_index_, 
                it->frame_type, 
                it->presentation_time, 
                it->frame_size,
                it->buf);
        }
        
        delete[] it->buf;
    }
    frame_queue_.clear();
        
}
void MediaOutputSink::ClearQueue()
{
    FrameQueue::iterator it;
    for(it= frame_queue_.begin(); it!= frame_queue_.end(); it++){
        delete[] it->buf;
    }
    frame_queue_.clear();
}
void MediaOutputSink::GetQueueFirstPts(struct timeval *presentationTime)
{
    if(presentationTime == NULL){
        return;
    }
    if(frame_queue_.size() == 0){
        presentationTime->tv_sec = 0;
        presentationTime->tv_usec = 0;
        return;
    }
    presentationTime->tv_sec = 
        frame_queue_.front().presentation_time.tv_sec;
    presentationTime->tv_usec = 
        frame_queue_.front().presentation_time.tv_usec;  
    
}


Boolean MediaOutputSink::continuePlaying() {
    if (fSource == NULL) return False; // sanity check (should not happen)

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(recv_buf_, sink_buf_size_,
                        afterGettingFrame, this,
                        onSourceClosure, this);
    return True; 
}