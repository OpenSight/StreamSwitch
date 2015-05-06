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

#define LOGGER_CHECK_INTERVAL 300 //300 sec


static void SignalHandler (int signal_num)
{
    RtspSourceApp * app = NULL;
    
    app = RtspSourceApp::Instance();

    app->SetWatch();
}

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
    scheduler_ = BasicTaskScheduler::createNew();
    env_ = BasicUsageEnvironment::createNew(*scheduler_); 
    prog_name_ = argv[0];
    
    //
    //install signal handler
    
    //
    //Parse options
        
    //Create Stream_switch source and init it
    
    
    //Create Rtsp Client    
    
    
    //install a logger check timer
    logger_check_task_ =
            env_->taskScheduler().scheduleDelayedTask(LOGGER_CHECK_INTERVAL * 1000000, 
				 (TaskFunc*)LoggerCheckHandler, this);
    
    is_init_ = true;
    
    return 0;
    
}
void RtspSourceApp::Uninit()
{
    is_init_ = false;
    
    //cancel the logger check timer
    
    //shutdown delete rtsp Client
    if(rtsp_client_ != NULL){
        rtsp_client_->Shutdown();
        delete rtsp_client_;
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