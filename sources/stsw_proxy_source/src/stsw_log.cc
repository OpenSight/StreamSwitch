/**
 * This file is part of stsw_proxy_source, which belongs to StreamSwitch
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
 * stsw_log.cc
 *      the implementation of log module
 * 
 * author: jamken
 * date: 2015-6-23
**/ 



#include "stsw_log.h"



stream_switch::RotateLogger * global_logger = NULL;
int stderr_level = 0;


int InitGlobalLogger(std::string base_name, 
                     int file_size, int rotate_num,  
                     int log_level)
{
    int ret = 0;
    global_logger = new stream_switch::RotateLogger();
    ret = global_logger->Init("stsw_proxy_source", 
            base_name, file_size, rotate_num, log_level, false);
    if(ret){
        delete global_logger;
        global_logger = NULL;
        fprintf(stderr, "Init Logger faile\n");
    }     
    stderr_level = log_level;
    return ret;
}

int UninitGlobalLogger()
{
    if(global_logger != NULL){
        global_logger->Uninit();
        delete global_logger;
        global_logger = NULL;
    }
}
