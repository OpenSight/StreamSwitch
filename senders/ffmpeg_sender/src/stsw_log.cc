/**
 * This file is part of ffmpeg_sender, which belongs to StreamSwitch
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
#include <syslog.h>

extern "C"{

#include <libavutil/avutil.h>    
    
}


stream_switch::RotateLogger * global_logger = NULL;
int stderr_level = stream_switch::LOG_LEVEL_DEBUG;

static int LogLevel2AvlogLevel(int log_level)
{
    using namespace stream_switch;   
    switch(log_level){
    case LOG_LEVEL_EMERG:
    case LOG_LEVEL_ALERT:
        return AV_LOG_PANIC;
    case LOG_LEVEL_CRIT:
        return AV_LOG_FATAL;
    case LOG_LEVEL_ERR:
        return AV_LOG_ERROR;
    case LOG_LEVEL_WARNING:
        return AV_LOG_WARNING;
    case LOG_LEVEL_NOTICE:
    case LOG_LEVEL_INFO:
        return AV_LOG_INFO;
    case LOG_LEVEL_DEBUG:   
        return AV_LOG_TRACE;
    }
}

static int AvlogLevel2LogLevel(int av_log_level)
{
    using namespace stream_switch;  
    if(av_log_level <=  AV_LOG_PANIC){
        return LOG_LEVEL_ALERT;        
    }else if (av_log_level <=  AV_LOG_FATAL){
        return LOG_LEVEL_CRIT;        
    }else if (av_log_level <=  AV_LOG_ERROR){
        return LOG_LEVEL_ERR;        
    }else if (av_log_level <=  AV_LOG_WARNING){
        return LOG_LEVEL_WARNING;        
    }else if (av_log_level <=  AV_LOG_INFO){
        return LOG_LEVEL_INFO;        
    }else if (av_log_level <=  AV_LOG_TRACE){
        return LOG_LEVEL_DEBUG;        
    }
    return LOG_LEVEL_INFO;
}

#define MAX_AV_LOG_SIZE 1024

static void AvlogCallback(void *avcl, int level, const char *fmt,
                     va_list vl)
{
    static int print_prefix = 1;
    char * tmp_av_log_buf = new char[MAX_AV_LOG_SIZE + 1];
    tmp_av_log_buf[MAX_AV_LOG_SIZE] = 0;
    av_log_format_line(avcl, level, fmt, vl, 
                       tmp_av_log_buf, MAX_AV_LOG_SIZE, &print_prefix);
    if(global_logger){
        int log_level;
        log_level = AvlogLevel2LogLevel(level);
        global_logger->Log(log_level, __FILE__, __LINE__, "%s", tmp_av_log_buf);
    }
        
    delete [] tmp_av_log_buf;
}

void SetLogLevel(int log_level)
{
    stderr_level = log_level;    
    av_log_set_level(LogLevel2AvlogLevel(log_level));    
}

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
        return ret;
    }
    
    av_log_set_callback(AvlogCallback);
    SetLogLevel(log_level);
   
    return ret;
}

int UninitGlobalLogger()
{
    if(global_logger != NULL){
        av_log_set_callback(av_log_default_callback);
        global_logger->Uninit();
        delete global_logger;
        global_logger = NULL;
    }
}
