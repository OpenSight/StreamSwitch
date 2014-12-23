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


namespace stream_switch {



class RotateLogger{
public:
    RotateLogger();
    virtual ~RotateLogger();
    
    virtual int Init(const std::string &prog_name, const std::string &base_name, 
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
    virtual void Log(int level, char * filename, int line, char * fmt, ...);
    
protected:

    virtual bool IsTooLarge();
    
    virtual void ShiftFile() ;
    virtual int Reopen();  
    virtual void CloseFile();
    
private:
    std::string prog_name_;
    std::string base_name_;
    int file_size_;
    int rotate_num_;
    bool stderr_redirect_;
    pthread_mutex_t lock_;
    int log_level_;
    int fd_;
    int redirect_fd_;    
    
// rotate logger flags
#define ROTATE_LOGGER_FLAG_INIT 1

    uint32_t flags_;      

};





inline bool RotateLogger::IsInit()
{
    return (flags_ & ROTATE_LOGGER_FLAG_INIT) != 0;        
}  

}

#endif