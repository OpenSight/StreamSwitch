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
 * stsw_h264or5_output_sink.cc
 *      H264or5OutputSink class implementation file, 
 * contains all the methods and logic of the H264or5OutputSink class
 * 
 * author: jamken
 * date: 2015-4-1
**/ 

#include "stsw_h264or5_output_sink.h"
#include "stsw_rtsp_client.h"

#include <unistd.h>

char const start_code[4] = {0x00, 0x00, 0x00, 0x01};


H264or5OutputSink* H264or5OutputSink::createNew(UsageEnvironment& env,
                                            LiveRtspClient *rtsp_client,
                                            MediaSubsession* subsession,                                          
                                            int32_t sub_stream_index,    
                                            size_t sink_buf_size, 
                                            int h_number)
{
    //sanity check
    if(subsession == NULL ||
       sub_stream_index < 0 ||
       sink_buf_size < 100 ||
       (h_number != 264 && h_number != 265)){
        return NULL;
    }
    
    return new H264or5OutputSink(env, rtsp_client, subsession, 
                               sub_stream_index, sink_buf_size, h_number);
}


H264or5OutputSink::H264or5OutputSink(UsageEnvironment& env, 
                    LiveRtspClient *rtsp_client,
                    MediaSubsession* subsession, 
                    int32_t sub_stream_index, size_t sink_buf_size, 
                    int h_number)
: MediaOutputSink(env, rtsp_client, subsession, sub_stream_index, sink_buf_size), 
h_number_(h_number)
{
    
    
    
}


H264or5OutputSink::~H264or5OutputSink() {
}



void H264or5OutputSink::DoAfterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds)
{
    using namespace stream_switch;
   
    if(presentationTime.tv_sec < last_pts_.tv_sec ||
       (presentationTime.tv_sec == last_pts_.tv_sec &&
        presentationTime.tv_usec < last_pts_.tv_usec)){

        // for h264/h265 stream, consider B frame situation 
        // presentation time is not required to monotone increasing
        
    }else{
        //update the last pts
        last_pts_.tv_sec = presentationTime.tv_sec;
        last_pts_.tv_usec = presentationTime.tv_usec;
    }
    
    H264or5VideoStreamFramer * source = 
        dynamic_cast<H264or5VideoStreamFramer *> (subsession_->readSource());
        
    if(source == NULL){
        //not H264or5VideoStreamFramer, just use default method
        memmove(recv_buf_, recv_buf_ + 4, frameSize); //recover recv_buf_ by removing the prepend start code 
        MediaOutputSink::DoAfterGettingFrame(frameSize, numTruncatedBytes, 
            presentationTime, durationInMicroseconds);
        return;
    }
    
    //clean the buf queue if overtime
    if(frame_queue_.size() != 0){
        double cur_frame_pts = (double)presentationTime.tv_sec + 
            ((double)presentationTime.tv_usec) / 1000000.0;
        struct timeval first_pts;
        GetQueueFirstPts(&first_pts);
        double first_frame_pts = (double)first_pts.tv_sec + 
            ((double)first_pts.tv_usec) / 1000000.0;        
        if(cur_frame_pts - first_frame_pts > 0.01){  //cache 10ms 
            ClearQueue();
        }
    }
    

    //update metadata if needed
    if(rtsp_client_ != NULL && 
       !rtsp_client_->IsMetaReady()){
        
        StreamMetadata *metadata = 
            rtsp_client_->mutable_metadata();
        if(metadata->sub_streams[sub_stream_index_].extra_data.size() == 0){
            u_int8_t* vps = NULL; unsigned vpsSize = 0;
            u_int8_t* sps = NULL; unsigned spsSize = 0;
            u_int8_t* pps = NULL; unsigned ppsSize = 0;
            bool isReady = false;
            source->getVPSandSPSandPPS(
                vps, vpsSize, sps, spsSize, pps, ppsSize);
            
            if(h_number_ == 264){
                if(sps != NULL && pps != NULL){
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append(start_code, 4);
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append((char const *)sps, spsSize);
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append(start_code, 4);   
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append((char const *)pps, ppsSize);
                      
                    //check metadata is ready after update
                    isReady = rtsp_client_->CheckMetadata();
                }
            
            }else if(h_number_ == 265){
                if(vps != NULL && sps != NULL && pps != NULL){
                    
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append(start_code, 4);
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append((char const *)vps, vpsSize); 
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append(start_code, 4);
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append((char const *)sps, spsSize);
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append(start_code, 4);   
                    metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append((char const *)pps, ppsSize);
                      
                    //check metadata is ready after update
                    isReady = rtsp_client_->CheckMetadata();
                }             
            }
            if(isReady){
                usleep(50 * 1000);//Hack!! delay sending the first frame for 50 ms.
                         //If send the first frame as soon as starting the stsw source
                         //, the receiver may lost it
            }
        }//if(metadata->sub_streams[sub_stream_index_].extra_data.size() == 0)
        
    }
    
    //analyze the frame's type
    u_int8_t nal_unit_type;
    if (h_number_ == 264 && frameSize >= 1) {
        nal_unit_type = recv_buf_[4]&0x1F;
    } else if (h_number_ == 265 && frameSize >= 2) {
        nal_unit_type = (recv_buf_[4]&0x7E)>>1;
    } else {
        // This is too short to be a valid NAL unit, so just assume a bogus nal_unit_type
        nal_unit_type = 0xFF;
    }    
    stream_switch::MediaFrameType frame_type;     
    if(IsVCL(nal_unit_type)){
        if(IsIDR(nal_unit_type)){
            frame_type = MEDIA_FRAME_TYPE_KEY_FRAME;   
        }else{
            frame_type = MEDIA_FRAME_TYPE_DATA_FRAME;               
        }        
    }else{
        frame_type = MEDIA_FRAME_TYPE_PARAM_FRAME;        
    }
    
    
    //calculate the lost frames
    CheckLostByTime(frame_type, presentationTime); 
    
    
    if(rtsp_client_ != NULL){
        if(rtsp_client_->IsMetaReady()){
            //flush frame cache first
            if(frame_queue_.size() != 0){
                FlushQueue();
            }
            
            //callback the parent rtsp client frame receive interface
            if(rtsp_client_ != NULL){                
                //frameSize + 4 to include the prepend start code
                rtsp_client_->AfterGettingFrame(sub_stream_index_, frame_type, 
                                                presentationTime, frameSize + 4, (const char *)recv_buf_);

            }
        }else{
            
            if(frame_type == MEDIA_FRAME_TYPE_PARAM_FRAME){
                //only buffer the param frame
                PushOneFrame(frame_type, 
                             presentationTime, 
                             frameSize + 4, (const char *)recv_buf_);
            }
            //drop the frame; 
        }
        
    }
    
        
}

Boolean H264or5OutputSink::continuePlaying() {
    if (fSource == NULL) return False; // sanity check (should not happen)
    
    //h264/h265 need prepend the start code
    memcpy(recv_buf_, start_code, 4);
    
    // Request the next frame of data from our input source.  
    //"afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(recv_buf_ + 4, sink_buf_size_ - 4,
                        afterGettingFrame, this,
                        onSourceClosure, this);
    return True; 
}






Boolean H264or5OutputSink::IsVCL(u_int8_t nal_unit_type)
{
  return h_number_ == 264
    ? (nal_unit_type <= 5 && nal_unit_type > 0)
    : (nal_unit_type <= 31);    
}


Boolean H264or5OutputSink::IsIDR(u_int8_t nal_unit_type)
{
  return h_number_ == 264
    ? (nal_unit_type == 5)
    : (nal_unit_type == 19 || nal_unit_type == 20);    

}
