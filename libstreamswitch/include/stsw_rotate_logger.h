/**
 * This file is part of libstreamswtich, which belongs to StreamSwitch
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
 * stsw_rotate_logger.h
 *      RotateLogger class header file, declare all interfaces of RotateLogger.
 * 
 * author: jamken
 * date: 2014-12-8
**/ 


#ifndef STSW_ROTATE_LOGGER_H
#define STSW_ROTATE_LOGGER_H
#include<map>
#include<stsw_defs.h>
#include<stdint.h>
#include<pthread.h>
#include<sys/time.h>
#include<stdarg.h>

namespace stream_switch {

// the Rotate logger class
//     A  Rotate logger is used to log the output message to a specified on-disk file with rotate function. 
// When the log file reach the specified size, it would be automatic rotates and keep the fix number rotate 
// log files. This rotate logger also providing the feature of redirecting stderr to its log file so that
// the application which outputs its message to stderr can make use of this logger without changing its code
// Thread safety: 
//     most of methods(excepts for init/uninit) are thread safe, which means
// multi threads can invoke its methods on the same instance of this class 
// simultaneously without additional lock mechanism
// Note: 
//     Only work for linux filesystem

class RotateLogger{
public:
    RotateLogger();
    virtual ~RotateLogger();
    
    virtual int Init(std::string prog_name, std::string base_name, 
                     int file_size, int file_num,  
                     int log_level, 
                     bool stderr_redirect);
    virtual void Uninit();


    //flag check methods
    virtual bool IsInit();
    
    //accessors
    virtual void set_log_level(int log_level);
    virtual int log_level();
    
    
    virtual void CheckRotate();
    virtual void Log(int level, const char * filename, int line, const char * fmt, ...);
    virtual void LogV(int level, const char * filename, int line, const char * fmt, va_list args);
    
protected:

    virtual bool IsTooLarge();
    
    virtual void ShiftFile() ;
    virtual int OpenFile();  
    virtual void CloseFile();
    virtual bool CheckFork();
    virtual void CheckRotateInternal();
    
private:
    std::string prog_name_;
    std::string base_name_;
    int file_size_;
    int rotate_num_;
    bool stderr_redirect_;
    pthread_mutex_t lock_;
    int log_level_;
    int fd_;
    int redirect_fd_old_; 

    
// rotate logger flags
#define ROTATE_LOGGER_FLAG_INIT 1

    volatile uint32_t flags_;   

    pid_t pid_;

};

inline bool RotateLogger::IsInit()
{
    return (flags_ & ROTATE_LOGGER_FLAG_INIT) != 0;        
}  


}


#define ROTATE_LOG(logger, level, fmt, ...)  \
do {         \
    if(logger){                  \
        logger->Log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__);   \
    }                            \
}while(0)

#endif