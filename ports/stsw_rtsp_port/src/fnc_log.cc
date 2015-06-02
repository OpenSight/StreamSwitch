/* *
 * This file is part of Feng
 *
 * Copyright (C) 2009 by LScube team <team@lscube.org>
 * See AUTHORS for more details
 *
 * feng is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * feng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with feng; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * */

#include <config.h>

#include <stdio.h>
#include <time.h>
#include <string.h>
#ifndef __WIN32__
#include <syslog.h>
#endif
#include <stdarg.h>

#include "fnc_log.h"

#include <string>


#define LOG_FORMAT "%d/%b/%Y:%H:%M:%S %z"
#define ERR_FORMAT "%a %b %d %H:%M:%S %Y"
#define MAX_LEN_DATE 30

static FILE *fd = NULL;

static int log_level = FNC_LOG_WARN;



static const char *log_level_str[] = 
{
    "FATAL", 
    "ERR", 
    "WARN", 
    "INFO", 
    "CLIENT",
    "DEBUG", 
    "VERBOSE", 
}; 


/**
 * Log to file descriptor
 * @brief print on standard error or file
 */
static void fnc_errlog(int level, const char * file, int line, const char *fmt, va_list args)
{

    
    if (level > log_level) return;

    ::std::string log_str;
    int ret;
    char time_str_buf[32];
    time_t curtime = time (NULL);
    char *time_str;
    time_str = ctime_r(&curtime, time_str_buf);    
    time_str_buf[strlen(time_str_buf) - 1] = 0; //remove the end NEWLINE char
#define MAX_LOG_RECORD_SIZE 1024    
    //
    // make up the log record string
    char * tmp_buf = new char[MAX_LOG_RECORD_SIZE];
    tmp_buf[MAX_LOG_RECORD_SIZE - 1] = 0;
    ret = snprintf(tmp_buf, MAX_LOG_RECORD_SIZE - 1, 
             "[%s][%s][%s:%d]: ",
             time_str, 
             log_level_str[level], 
             file, line);
    if(ret < 0){
        goto out;
    }
    log_str += tmp_buf;
    

    ret = vsnprintf (tmp_buf, MAX_LOG_RECORD_SIZE - 1, fmt, args);
    if(ret < 0){
        goto out;
    }
  
    log_str += tmp_buf;
    if(log_str[log_str.length() - 1] != '\n'){
        log_str.push_back('\n');
    }
       
    //
    //print log to log file

 
      
    if(fd){
        fwrite(log_str.c_str(), log_str.length(), 1, fd); 
        fflush(fd);           
    }
    
    
out:
    delete[] tmp_buf;

}


static void fnc_syslog(int level, const char * file, int line, const char *fmt, va_list args)
{
#ifndef __WIN32__    
    int l = LOG_ERR;

    if (level > log_level) return;

    switch (level) {
        case FNC_LOG_FATAL:
            l = LOG_CRIT;
            break;
        case FNC_LOG_ERR:
            l = LOG_ERR;
            break;
        case FNC_LOG_WARN:
            l = LOG_WARNING;
            break;
        case FNC_LOG_DEBUG:
            l = LOG_DEBUG;
            break;
        case FNC_LOG_VERBOSE:
            l = LOG_DEBUG;
            break;
        case FNC_LOG_CLIENT:
            l = LOG_INFO;
            break;
        default:
            l = LOG_INFO;
            break;
    }
    vsyslog(l, fmt, args);
#endif    
}


static void (*fnc_vlog)(int, const char * , int , const char*, va_list) = fnc_errlog;

/**
 * Set the logger and returns the function pointer to be feed to the
 * Sock_init
 * @param file path to the logfile
 * @param out specifies the logger function
 * @param name specifies the application name
 * @return the logger currently in use
 * */
fnc_log_t fnc_log_init(char *file, int out, int level, char *name)
{
    fd = stderr;
    log_level = level;
    switch (out) {
        case FNC_LOG_SYS:
#ifndef __WIN32__        
            openlog(name, LOG_PID /*| LOG_PERROR*/, LOG_DAEMON);
            fnc_vlog = fnc_syslog;
#endif            
            break;
        case FNC_LOG_FILE:
            //if ((fd = fopen(file, "a+")) == NULL) fd = stderr;
            break;
        case FNC_LOG_OUT:
            fd = stderr;
            fnc_vlog = fnc_errlog;
            break;
    }
    return NULL;
}


fnc_log_t fnc_rotate_log_init(char *prog_name, char *file, 
                              int level, int file_size, int rotate_num)
{
    fd = 0;
    log_level = level;

    return NULL;
}


/**
 * External logger function
 * @param level log level
 * @param fmt as printf format string
 * @param ... as printf variable argument
 */
void fnc_log_internal(int level, const char * file, int line, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    if(fnc_vlog != NULL){
        fnc_vlog(level, file, line, fmt, vl);
        
    }
    va_end(vl);    
}


void fnc_log_uninit(void)
{
#ifndef __WIN32__    
    if(fnc_vlog == fnc_syslog){
        closelog();
    }
#endif    
    fd = stderr;
    fnc_vlog = fnc_errlog;    
}


