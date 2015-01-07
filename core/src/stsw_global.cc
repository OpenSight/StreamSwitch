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
 * stsw_global.cc
 *      stream switch core lib global function implementation.
 * 
 * author: jamken
 * date: 2015-1-3
**/ 

#include <stsw_global.h>
#include <google/protobuf/stubs/common.h>
#include <czmq.h>



namespace stream_switch {

    
int GlobalInit()
{
    int ret;
    
    
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.    
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    //init zeromq, and install default signal handler
    zsys_init();
    
    return 0;
}    
    


void GlobalUninit()
{
 
    //deinit zeromq   
    zsys_handler_reset(); 
    zsys_shutdown();

    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
    
}



bool isGlobalInterrupt()
{
    return (zsys_interrupted != 0);
}
    

void SetGolbalInterrupt(bool isInterrupt)
{
    if(isInterrupt){
        zsys_interrupted = 1;
    }else{
        zsys_interrupted = 0;
    }
}


void SetIntSigHandler(SignalHandler handler)
{
    zsys_handler_set((zsys_handler_fn *)handler);
}


}





