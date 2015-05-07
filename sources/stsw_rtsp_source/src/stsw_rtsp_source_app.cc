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
 * stsw_rtsp_source_app.cc
 *      RtspSourceApp class implementation file, 
 * contains all the methods and logic of the RtspSourceApp class
 * 
 * author: jamken
 * date: 2015-5-6
**/ 

#include "stsw_rtsp_source_app.h"

#include "BasicUsageEnvironment.hh"

#define LOGGER_CHECK_INTERVAL 600 //600 sec


static void SignalHandler (int signal_num)
{
    RtspSourceApp * app = NULL;
    
    app = RtspSourceApp::Instance();

    app->SetWatch();
}


RtspSourceApp * RtspSourceApp::s_instance = NULL;


RtspSourceApp::RtspSourceApp()
: rtsp_client_(NULL), scheduler_(NULL), env_(NULL), watch_variable_(0), 
is_init_(false), exit_code_(0), logger_check_task_(NULL)
{
    
}

RtspSourceApp::~RtspSourceApp()
{
    

}

int RtspSourceApp::Init(int argc, char ** argv)
{
    int ret;
    scheduler_ = BasicTaskScheduler::createNew();
    env_ = BasicUsageEnvironment::createNew(*scheduler_); 
    prog_name_ = argv[0];
    
    //
    //install signal handler
    
    //
    //Parse options
        
    //Create Stream_switch source and init it
    
    
    //Create Rtsp Client    
    rtsp_client_ = LiveRtspClient::CreateNew(
        *env_, (char *)"rtsp://172.16.56.120:554/user=admin&password=123456&id=1&type=0",  True, True, 
        NULL,  NULL, NULL, (LiveRtspClientListener *)this, 1);
    if(rtsp_client_ == NULL){
        fprintf(stderr, "LiveRtspClient::CreateNew() Failed: Maybe parameter error\n");
        ret = -1;
        goto error_out1;
    }
    
    
    //install a logger check timer
    logger_check_task_ =
            env_->taskScheduler().scheduleDelayedTask(10 * 1000000, 
				 (TaskFunc*)LoggerCheckHandler, this);
    
    is_init_ = true;
    
    return 0;

error_out1:
    return ret;
    
}
void RtspSourceApp::Uninit()
{
    is_init_ = false;
    
    //cancel the logger check timer
    
    //shutdown delete rtsp Client
    if(rtsp_client_ != NULL){
        rtsp_client_->Shutdown();
        Medium::close(rtsp_client_);
        rtsp_client_ = NULL;
    }
    
    //uninit stream_switch source
    
    if(env_ != NULL) {
        env_->reclaim();
        env_ = NULL;
    }
    
    if(scheduler_ != NULL){
        delete scheduler_;
        scheduler_ = NULL;
    }    
    
}
int RtspSourceApp::DoLoop()
{
    if(!is_init_){
        fprintf(stderr, "RtspSourceApp Not Init\n");
        return -1;
    }    
    
    rtsp_client_->Start();
    
    
    env_->taskScheduler().doEventLoop(&watch_variable_);
    
    
    //shutdown rtsp client
    rtsp_client_->Shutdown(); 
    
    //shutdown source
    
    return exit_code_;
}


//////////////////////////////////////////////////////////
//Timer task handler
void RtspSourceApp::LoggerCheckHandler(void* clientData)
{
    RtspSourceApp * app = (RtspSourceApp *)clientData;
    //check log to big
    app->exit_code_ = 0;
    app->SetWatch();
    
}




///////////////////////////////////////////////////////////
// LiveRtspClientListener implementation
void RtspSourceApp::OnMediaFrame(
        const stream_switch::MediaFrameInfo &frame_info, 
        const char * frame_data, 
        size_t frame_size
)
{
    fprintf(stderr, "RtspSourceApp::OnMediaFrame() is called with the below frame:\n");
    fprintf(stderr, 
                "index:%d, type:%d, time:%lld.%03d, ssrc:0x%x, size: %d\n", 
                (int)frame_info.sub_stream_index, 
                frame_info.frame_type, 
                (long long)frame_info.timestamp.tv_sec, 
                (int)(frame_info.timestamp.tv_usec/1000), 
                (unsigned)frame_info.ssrc, 
                (int)(frame_size));
    
}
void RtspSourceApp::OnError(RtspClientErrCode err_code, const char * err_info)
{
    fprintf(stderr, "RtspSourceApp::OnError() is called with RtspClientErrCode err_code(%d):%s\n",
            err_code, (err_info!=NULL)? err_info:"");
    exit_code_ = -1;
    SetWatch();
}

void RtspSourceApp::OnMetaReady(const stream_switch::StreamMetadata &metadata)
{
    fprintf(stderr, "RtspSourceApp::OnMetaReady() is called with the below metadata:\n");
    fprintf(stderr, 
            "play_type:%d, source_proto:%s, ssrc:0x%x, bps: %d, substream number: %d\n", 
            (int)metadata.play_type,  
            metadata.source_proto.c_str(), 
            (unsigned)metadata.ssrc, 
            (int)metadata.bps,
            (int)(metadata.sub_streams.size()));    
            
    stream_switch::SubStreamMetadataVector::const_iterator it;
    for(it = metadata.sub_streams.begin();
        it != metadata.sub_streams.end();
        it++){  
      
        fprintf(stderr, 
                "Stream %u --- media_type:%d, codec_name:%s, direction:%d, extra_data size:%d\n", 
                it->sub_stream_index, 
                (int)it->media_type,
                it->codec_name.c_str(), 
                (int)it->direction, 
                (int)it->extra_data.size());                
            
    }
    fprintf(stderr, "\n");
}
void RtspSourceApp::OnRtspOK()
{
    fprintf(stderr, "RtspSourceApp::OnRtspOK() is called\n");    
    
}
    
