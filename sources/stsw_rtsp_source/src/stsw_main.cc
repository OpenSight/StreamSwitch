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
 * stsw_main.cc
 *      the main source file of stsw_rtsp_soure program
 * 
 * author: OpenSight Team
 * date: 2015-4-29
**/ 



#include "stsw_rtsp_source_app.h"
#include "stream_switch.h"


static void SignalHandler (int signal_num)
{
    RtspSourceApp * app = NULL;
    
    stream_switch::SetGolbalInterrupt(true);
    
    app = RtspSourceApp::Instance();    

    app->SetWatch();
}


int main(int argc, char ** argv)
{
    int ret = 0;
    
    stream_switch::GlobalInit();
    
    RtspSourceApp * app = NULL;
    
    app = RtspSourceApp::Instance();
    
    ret = app->Init(argc, argv);
    if(ret){
        goto err_1;
    }
    
    //install signal handler
    stream_switch::SetIntSigHandler(SignalHandler); 
    
    ret = app->DoLoop();
        
    app->Uninit();
        
err_1:    

    RtspSourceApp::Uninstance();
    
    stream_switch::GlobalUninit();
    
    fprintf(stderr, "stsw_rtsp_source exit with code:%d\n",ret);
    
    return ret;
}