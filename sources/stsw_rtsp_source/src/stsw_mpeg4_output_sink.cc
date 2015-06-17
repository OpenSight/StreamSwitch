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
 * stsw_mpeg4_output_sink.cc
 *      Mpeg4OutputSink class implementation file, 
 * contains all the methods and logic of the Mpeg4OutputSink class
 * 
 * author: jamken
 * date: 2015-4-1
**/ 

#include "stsw_mpeg4_output_sink.h"
#include "stsw_rtsp_client.h"

#include <unistd.h>


Mpeg4OutputSink* Mpeg4OutputSink::createNew(UsageEnvironment& env,
                                            LiveRtspClient *rtsp_client,
                                            MediaSubsession* subsession,                                          
                                            int32_t sub_stream_index,    
                                            size_t sink_buf_size)
{
    //sanity check
    if(subsession == NULL ||
       sub_stream_index < 0 ||
       sink_buf_size < 100 ){
        return NULL;
    }
    
    return new Mpeg4OutputSink(env, rtsp_client, subsession, 
                               sub_stream_index, sink_buf_size);
}


Mpeg4OutputSink::Mpeg4OutputSink(UsageEnvironment& env, 
                    LiveRtspClient *rtsp_client,
                    MediaSubsession* subsession, 
                    int32_t sub_stream_index, size_t sink_buf_size)
: MediaOutputSink(env, rtsp_client, subsession, sub_stream_index, sink_buf_size)
{

}


Mpeg4OutputSink::~Mpeg4OutputSink() {

}



void Mpeg4OutputSink::DoAfterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                struct timeval presentationTime, unsigned durationInMicroseconds)
{
    using namespace stream_switch;


    if(presentationTime.tv_sec < last_pts_.tv_sec ||
       (presentationTime.tv_sec == last_pts_.tv_sec &&
        presentationTime.tv_usec < last_pts_.tv_usec)){

        // for mpeg4 stream, consider B frame situation 
        // presentation time is not required to monotone increasing
        
    }else{
        //update the last pts
        last_pts_.tv_sec = presentationTime.tv_sec;
        last_pts_.tv_usec = presentationTime.tv_usec;
    }


    
    MPEG4VideoStreamDiscreteFramer * source = 
        dynamic_cast<MPEG4VideoStreamDiscreteFramer *> (subsession_->readSource());
        
    if(source == NULL){
        //not MPEG4VideoStreamDiscreteFramer, just use default method
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
    
#if 1
    //update metadata if needed
    if(rtsp_client_ != NULL && 
       !rtsp_client_->IsMetaReady()){
        
        StreamMetadata *metadata = 
            rtsp_client_->mutable_metadata();
        if(metadata->sub_streams[sub_stream_index_].extra_data.size() == 0){
            bool isReady = false;
            unsigned char* config;
            unsigned configLength;
            config = source->getConfigBytes(configLength);
            if(config != NULL && configLength != 0){
                metadata->sub_streams[sub_stream_index_]. \
                      extra_data.append((const char *)config, configLength);
                      
                //check metadata is ready after update
                isReady = rtsp_client_->CheckMetadata();
            }            
            if(isReady){
                usleep(50 * 1000);//Hack!! delay sending the first frame for 50 ms.
                         //If send the first frame as soon as starting the stsw source, 
                         //the receiver may lost it
            }
        }//if(metadata->sub_streams[sub_stream_index_].extra_data.size() == 0)
        
    }
#endif
    
    //analyze the frame's type

    stream_switch::MediaFrameType frame_type;     
    if(IsIncludeVOP(recv_buf_, frameSize)){
        if(IsKeyFrame(recv_buf_, frameSize)){
            frame_type = MEDIA_FRAME_TYPE_KEY_FRAME;   
        }else{
            frame_type = MEDIA_FRAME_TYPE_DATA_FRAME;               
        }        
    }else{
        if(frameSize < 1024){
            frame_type = MEDIA_FRAME_TYPE_PARAM_FRAME;        
            
        }else{
            frame_type = MEDIA_FRAME_TYPE_DATA_FRAME;  
        }
    }
    
    //calculate the lost frames
    CheckLostByTime(frame_type, presentationTime);    
    
    if(rtsp_client_!= NULL){
        if(rtsp_client_->IsMetaReady()){
            //flush frame cache first
            if(frame_queue_.size() != 0){
                FlushQueue();
            }
            
            //callback the parent rtsp client frame receive interface
            if(rtsp_client_ != NULL){
                rtsp_client_->AfterGettingFrame(sub_stream_index_, frame_type, 
                                                presentationTime, frameSize, (const char *)recv_buf_);
            }
        }else{
            
            if(frame_type == MEDIA_FRAME_TYPE_PARAM_FRAME){
                //only buffer the param frame
                PushOneFrame(frame_type, 
                             presentationTime, 
                             frameSize, (const char *)recv_buf_);
            }
            //drop the frame; 
        }
    }
        
}







#define GROUP_VOP_START_CODE      0xB3
#define VOP_START_CODE            0xB6
#define VISUAL_OBJECT_SEQUENCE_START_CODE      0xB0
#define VOP_CODING_TYPE_I         0
#define VOP_CODING_TYPE_P         1
#define VOP_CODING_TYPE_B         2
#define VOP_CODING_TYPE_S         3




Boolean Mpeg4OutputSink::IsKeyFrame(unsigned char * buf, unsigned buf_size)
{
    unsigned i;
    unsigned char vop_coding_type;

    if(buf == NULL || buf_size < 4) {
        return False;
    }
    
    for (i = 3; i < buf_size; ++i) {
        if (buf[i-1] == 1 && buf[i-2] == 0 && buf[i-3] == 0) {
            switch(buf[i]) {
            case VISUAL_OBJECT_SEQUENCE_START_CODE:
            case GROUP_VOP_START_CODE:
                return True;
                break;
            case VOP_START_CODE:
                if(i+1 < buf_size ) {
                    vop_coding_type = buf[i+1] >> 6;
                    if(vop_coding_type == VOP_CODING_TYPE_I) {
                        return True;
                    }else{
                        return False;
                    }
                }
                break;
            }

        }
    }    
        
    return False;      
}



Boolean Mpeg4OutputSink::IsIncludeVOP(unsigned char * buf, unsigned buf_size)
{
    unsigned i;

    if(buf == NULL || buf_size < 4) {
        return False;
    }
    
    for (i = 3; i < buf_size; ++i) {
        if (buf[i] == VOP_START_CODE 
            && buf[i-1] == 1 && buf[i-2] == 0 && buf[i-3] == 0) {
            return True;
        }
    }    
        
    return False;    

}