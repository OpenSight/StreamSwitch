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
 * stsw_rtsp_source_app.h
 *      RtspSourceApp class header file, declare all the interface of 
 *  RtspSourceApp 
 * 
 * author: jamken
 * date: 2015-5-6
**/ 


#ifndef STSW_RTSP_SOURCE_APP_H
#define STSW_RTSP_SOURCE_APP_H


#include "stream_switch.h"
#include "stsw_rtsp_client.h"



////////////////////

// the RTSP Source Application class
//    This class reprensent a stream switch source application for RTSP protocol, 
// which is composed of a RTSP client and a stream switch source object. 
//    General speaking, the class is responsible for getting media frames 
// from the RTSP client and send them to the source. 
class RtspSourceApp: public LiveRtspClientListener
{
public:
	static RtspSourceApp* Instance()
	{ 
		if ( NULL == s_instance )
		{
			s_instance = new RtspSourceApp();
		}

		return s_instance;
	}

	static void Uninstance()
	{
		if ( NULL != s_instance )
		{
			delete s_instance;
			s_instance = 0;
		}
	}
    
    
public:
    
 


    virtual int Init(int argc, char ** argv);
    virtual void Uninit();
    virtual int DoLoop();
    
    virtual void SetWatch(){
        watch_variable_ = 1;
    }
   
    ///////////////////////////////////////////////////////////
    // LiveRtspClientListener implementation
    virtual void OnMediaFrame(
        const stream_switch::MediaFrameInfo &frame_info, 
        const char * frame_data, 
        size_t frame_size
    );
    virtual void OnError(RtspClientErrCode err_code, const char * err_info);
    virtual void OnMetaReady(const stream_switch::StreamMetadata &metadata);  
    virtual void OnRtspOK();  
    
    //////////////////////////////////////////////////////////
    //Timer task handler
    static void LoggerCheckHandler(void* clientData);



private:
    RtspSourceApp();
    virtual ~RtspSourceApp();   
    
    static RtspSourceApp * s_instance;

    LiveRtspClient * rtsp_client_;
    TaskScheduler * scheduler_;
    UsageEnvironment* env_;
    std::string prog_name_;
    char  watch_variable_;
    bool is_init_;
    int exit_code_;
    TaskToken logger_check_task_;
    
};


#endif
