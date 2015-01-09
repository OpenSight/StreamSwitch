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
 * stsw_rotate_logger.cc
 *      RotateLogger class implementation file, define all methods of RotateLogger.
 * 
 * author: jamken
 * date: 2014-12-8
**/ 

#include <stsw_rotate_logger.h>
#include <stdint.h>
#include <list>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#include <stsw_lock_guard.h>


#define STDERR_FD   2 


namespace stream_switch {

    
    

static const char *log_level_str[] = 
{
    "EMERG", 
    "ALERT", 
    "CRIT", 
    "ERR", 
    "WARNING",
    "NOTICE", 
    "INFO", 
    "DEBUG",
};    
    

RotateLogger::RotateLogger()
:file_size_(0), rotate_num_(0), stderr_redirect_(false), 
log_level_(0), fd_(-1), redirect_fd_old_(-1), flags_(0)
{
    
}



RotateLogger::~RotateLogger()
{
    Uninit();
}

    
int RotateLogger::Init(const std::string &prog_name, const std::string &base_name, 
                       int file_size, int rotate_num,  
                       int log_level, 
                       bool stderr_redirect)
{
    int ret;
    
    if(file_size <= 0 || rotate_num < 0){
        ret = ERROR_CODE_PARAM;
        return ret;
    }
    
    if((flags_ & ROTATE_LOGGER_FLAG_INIT) != 0){
        return ERROR_CODE_GENERAL;
    }    
    
    prog_name_ = prog_name;
    base_name_ = base_name;
    file_size_ = file_size;
    rotate_num_ = rotate_num;

    if(log_level < LOG_LEVEL_EMERG){
        log_level = LOG_LEVEL_EMERG;
    }else if(log_level > LOG_LEVEL_DEBUG) {
        log_level = LOG_LEVEL_DEBUG;        
    }    
    log_level_ = log_level; 

    stderr_redirect_ = stderr_redirect;
    if(stderr_redirect_){
        //back up original stderr fd
        redirect_fd_old_ = dup(STDERR_FD);
        if(redirect_fd_old_ < 0){
            ret = ERROR_CODE_SYSTEM;
            goto error_0;
        }
    }
    

    //init lock
    pthread_mutexattr_t  attr;
    pthread_mutexattr_init(&attr);
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  
    if(ret){
        ret = ERROR_CODE_SYSTEM;
        perror("pthread_mutexattr_settype failed");   
        pthread_mutexattr_destroy(&attr);      
        goto error_1;
        
    }    
    ret = pthread_mutex_init(&lock_, &attr);  
    if(ret){
        ret = ERROR_CODE_SYSTEM;
        perror("pthread_mutex_init failed");
        pthread_mutexattr_destroy(&attr); 
        goto error_1;
    }
    pthread_mutexattr_destroy(&attr);   

    
    
    ret = Reopen();
    if(ret){
        perror("Open log file failed"); 
        goto error_2;
    }        
    flags_ |= ROTATE_LOGGER_FLAG_INIT;  
   
    return 0;
    
error_2:

    pthread_mutex_destroy(&lock_);   
    
error_1:    
    if(redirect_fd_old_ >= 0){
        close(redirect_fd_old_);
        redirect_fd_old_ = -1;
    }
    
error_0:
    //clean field
    prog_name_.clear();
    base_name_.clear();
    file_size_ = 0;
    rotate_num_ = 0;
    log_level_ = LOG_LEVEL_EMERG; 
    stderr_redirect_ = false;
    redirect_fd_old_ = -1; 

    
    return ret;
}
         
void RotateLogger::Uninit()
{
    
    if((flags_ & ROTATE_LOGGER_FLAG_INIT) == 0){
        //not init yet or already uninit
        return;
    }  
    
    CloseFile();

    pthread_mutex_destroy(&lock_);   
    
    if(redirect_fd_old_ >= 0){
        //restore the original stderr fd
        dup2(redirect_fd_old_, STDERR_FD);
        close(redirect_fd_old_);
        redirect_fd_old_ = -1;
    }    
    

    //clean field
    prog_name_.clear();
    base_name_.clear();
    file_size_ = 0;
    rotate_num_ = 0;
    log_level_ = LOG_LEVEL_EMERG; 
    stderr_redirect_ = false;


    flags_ &= ~(ROTATE_LOGGER_FLAG_INIT); 
    
}  
  


void RotateLogger::set_log_level(int log_level)
{
    if(log_level < LOG_LEVEL_EMERG){
        log_level = LOG_LEVEL_EMERG;
    }else if(log_level > LOG_LEVEL_DEBUG) {
        log_level = LOG_LEVEL_DEBUG;        
    }    
    
    log_level_ = log_level;
}
int RotateLogger::log_level()
{
    return log_level_;
}
    
    
void RotateLogger::CheckRotate()
{
    if(!IsInit()){
        return;
    }
    
    LockGuard guard(&lock_);  
    
    if(IsTooLarge()){
        ShiftFile();
        Reopen();        
    }
}

#define MAX_LOG_RECORD_SIZE 1024

void RotateLogger::Log(int level, const char * filename, int line, const char * fmt, ...)
{
    
    if(!IsInit()){
        return;
    }   
    
    if(level > log_level_){
        return; // level filter
    }
        
    ::std::string log_str;
    int ret;
    char time_str_buf[32];
    time_t curtime = time (NULL);
    char *time_str;
    time_str = ctime_r(&curtime, time_str_buf);    
    
    //
    // make up the log record string
    char * tmp_buf = new char[MAX_LOG_RECORD_SIZE];
    tmp_buf[MAX_LOG_RECORD_SIZE - 1] = 0;
    ret = snprintf(tmp_buf, MAX_LOG_RECORD_SIZE - 1, 
             "[%s][%s][%s][%s:%d]:",
             time_str, 
             prog_name_.c_str(), 
             log_level_str[level], 
             filename, line);
    if(ret < 0){
        goto out;
    }
    log_str += tmp_buf;
    
    va_list args;
    va_start (args, fmt);
    ret = vsnprintf (tmp_buf, MAX_LOG_RECORD_SIZE - 1, fmt, args);
    if(ret < 0){
        goto out;
    }
    va_end (args);   
    log_str += tmp_buf;
    if(log_str[log_str.length() - 1] != '\n'){
        log_str.push_back('\n');
    }
    

    
    //
    //print log to log file
    {
        //check if need rotate
        CheckRotate();  
      
        LockGuard guard(&lock_);  
        if(fd_ >= 0){
            write(fd_, log_str.c_str(), log_str.length());            
        }
    }
    
out:
    delete[] tmp_buf;
    
}
    
bool RotateLogger::IsTooLarge()
{
    if(fd_ < 0){
        // no open log file
        return false;
    }
    
    off_t len = lseek(fd_, 0, SEEK_CUR);
    if (len < 0) {
        return false;
    }

    return (len >= file_size_);
  
}
    
void RotateLogger::ShiftFile()
{
    
    char log_file[1024];
    char log_file_old[1024];
    memset(log_file, 0, 1024);
    memset(log_file_old, 0, 1024);    
    
    //
    // remove the last rotate file
    if(rotate_num_ > 0){
        snprintf(log_file, sizeof(log_file)-1, "%s.%d", base_name_.c_str(), rotate_num_);        
    }else{
        snprintf(log_file, sizeof(log_file)-1, "%s", base_name_.c_str());        
    }
    remove(log_file);
    
    //
    //change the file name;
    char *src=log_file_old;
    char *dst=log_file;
    char *tmp = NULL;
    for (int i = rotate_num_ - 1; i >= 0; i--) {
        if(i > 0){
            snprintf(src, sizeof(log_file) -1, "%s.%d", base_name_.c_str(), i);            
        }else{
            snprintf(src, sizeof(log_file) -1, "%s", base_name_.c_str());
        }        
        rename(src,dst);
        tmp = dst;
        dst = src;
        src = tmp;
    }
}

int RotateLogger::Reopen()
{
    
    CloseFile();
    
    int fd = open(base_name_.c_str(), O_CREAT|O_APPEND|O_WRONLY, 0777);
    if (fd < 0) {
        
        if(stderr_redirect_){
            dup2(redirect_fd_old_, STDERR_FD);
        }                   
        return ERROR_CODE_SYSTEM;
    }else{

        if(stderr_redirect_){
            dup2(fd, STDERR_FD);
        }        
    }
    

    fd_ = fd;
    
    return 0;
}

void RotateLogger::CloseFile()
{
    if(fd_ >= 0){
        ::close(fd_);
        fd_ = -1;
    }
        
}    

}





