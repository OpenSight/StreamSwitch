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
 * stsw_log.h
 *      the header file of the log module 
 * 
 * author: jamken
 * date: 2015-6-24
**/ 

#include <stream_switch.h>
#include <stdio.h>
#include <string>

extern stream_switch::RotateLogger * global_logger;


#define STDERR_LOG(level, fmt, ...)  \
do {         \
    if(global_logger != NULL){                  \
        global_logger->Log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__);   \
    }else{                 \
        fprintf(stderr, fmt, ##__VA_ARGS__);    \
    }                             \
}while(0)