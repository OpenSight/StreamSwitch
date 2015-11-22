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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/**
 * stsw_ffmpeg_arg_parser.cc
 *      FFmpegArgParser class implementation file, FFmpegArgParser is 
 * the an arg parser for ffmpeg_source application. FFmpegArgParser 
 * extends the ArgParser to parse the ffmpeg options which start by 
 * "--ffmpeg-". 
 * 
 * author: jamken
 * date: 2015-10-17
**/ 

#include "stsw_ffmpeg_arg_parser.h"
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>


#define FFMPEG_OPT_PRE  "--ffmpeg-"
    
    
FFmpegSenderArgParser::FFmpegSenderArgParser()

{

}


FFmpegSenderArgParser::~FFmpegSenderArgParser()
{
    
}


void FFmpegSenderArgParser::RegisterSenderOptions()
{
    RegisterOption("stream-name", 's', 
                   OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG, "STREAM",
                   "local stream name, if the user want to send out from "
                   "a local stream, this option should be used to set the "
                   "name of the stream", NULL, NULL);
    RegisterOption("host", 'H', OPTION_FLAG_WITH_ARG, "HOSTADDR", 
                   "remote host IP address, if the user want to "
                   "send out from a remote stream, this option should be used to "
                   "set the remote host address of this stream", NULL, NULL);                                      
    RegisterOption("port", 'p', OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,
                   "PORT", 
                   "remote tcp port, if the user want "
                   "send out from a remote stream, this option "
                   "should be used to set the remote tcp port of this stream", NULL, NULL);
    RegisterOption("log-file", 'l', OPTION_FLAG_WITH_ARG,  "FILE",
                   "log file path for debug, default is no log", NULL, NULL);   
    RegisterOption("log-size", 'L', OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,  
                   "SIZE",
                   "log file max size in bytes", NULL, NULL);   
    RegisterOption("log-rotate", 'r', OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,
                   "NUM",
                   "log rotate number, 0 means no rotating, default is 0", NULL, NULL);  
    RegisterOption("log-level", 0, OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,
                   "NUM",
                   "log level, compatible with syslog's log level number. "
                   "By default, it's LOG_INFO(6)", NULL, NULL);  
    RegisterOption("queue-size", 'q', OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,
                   "NUM",
                   "the size of the message queue for Subscriber, 0 means no limit."
                   "Default is an internal value determined when compiling", NULL, NULL);  
     
    RegisterOption("url", 'u', OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG,
                   "URL", 
                   "the url of the dest file which the sender send to", NULL, NULL);   
    
    //register the other options
    RegisterOption("io-timeout", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "SEC",
                   "timeout (in sec) for ffmpeg IO operation. Default is 10 sec", NULL, NULL);
    RegisterOption("inter-frame-gap", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "SEC",
                   "the max time (in sec) between the two consecutive frames "
                   "If the gap is over this limitation, this sender would exit with error. "
                   "Defualt is 20 sec", NULL, NULL);
/*                   
    RegisterOption("retry-count", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "COUNT",
                   "how many time would the sender try to initialize the dest file. "
                   "Defualt is 3", NULL, NULL);    
    RegisterOption("retry-interval", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "MS",
                   "The time interval (in millisecond) before reinitialize the dest file. "
                   "Defualt is 50 ms", NULL, NULL);      
*/    
    RegisterOption("duration", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "MS",
                   "how long (in millisecond) the sender does work for, "
                   "after this duration, the sender would stop sending data and exit with 0. "
                   "Defualt is 0 ms, means forever", NULL, NULL);   
    RegisterOption("ffmpeg-[NAME]", 0, 
                   OPTION_FLAG_WITH_ARG, "VALUE",
                   "user can used --ffmpeg-[optioan]=[value] form to pass the options to the ffmpeg library. "
                   "ffmpeg_sender parse the options which start with \"ffmpeg-\", " 
                   "and pass them to ffmpeg muxing context and io context. "
                   "The option name would be extracted from the string after \"ffmpeg-\", the value would be set to VALUE." ,
                   NULL, NULL);
    RegisterOption("debug-flags", 'd', 
                    OPTION_FLAG_LONG | OPTION_FLAG_WITH_ARG,  "FLAG", 
                    "debug flag for stream_switch core library. "
                    "Default is 0, means no debug dump" , 
                    NULL, NULL);  
    
}
    

bool FFmpegSenderArgParser::ParseUnknown(const char * unknown_arg)
{
    //just ignore, user can override this function to print error message and exit
    if(unknown_arg == NULL){
        return false;
    }
    
    if(strncmp(unknown_arg, FFMPEG_OPT_PRE, strlen(FFMPEG_OPT_PRE)) == 0){
        std::string ffmpeg_option = 
            std::string(unknown_arg + strlen(FFMPEG_OPT_PRE));
        if(ffmpeg_option.find('=') == std::string::npos){
            ffmpeg_option.append("=1");
        }
        if(ffmpeg_options_.size() != 0){
            ffmpeg_options_.append(",");
        
        }
        ffmpeg_options_.append(ffmpeg_option);  
        return true;
    }
    
    return false;
}





