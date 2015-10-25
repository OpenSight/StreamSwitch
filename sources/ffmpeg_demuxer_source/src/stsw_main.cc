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
 *      main entry file for ffmpeg_source
 * 
 * author: jamken
 * date: 2015-10-16
**/ 



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stream_switch.h>

extern "C"{
#include <libavformat/avformat.h>
   
}

#include "stsw_ffmpeg_source_global.h"
#include "stsw_ffmpeg_arg_parser.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_demuxer_source.h"

///////////////////////////////////////////////////////////////
//Type


//////////////////////////////////////////////////////////////
//global variables

int exit_code = 0;

///////////////////////////////////////////////////////////////
//macro





///////////////////////////////////////////////////////////////
//functions
    
void ParseArgv(int argc, char *argv[], 
               FFmpegArgParser *parser)
{
    int ret = 0;
    std::string err_info;

    parser->RegisterBasicOptions();
    parser->RegisterSourceOptions();
    
    parser->RegisterOption("url", 'u', OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG,
                   "URL", 
                   "the input file path or the network URL "
                   "which the source read media data from by ffmpeg demuxing library. "
                   "Should be a vaild input for ffmpeg's libavformat", NULL, NULL);  


    parser->RegisterOption("debug-flags", 'd', 
                    OPTION_FLAG_LONG | OPTION_FLAG_WITH_ARG,  "FLAG", 
                    "debug flag for stream_switch core library. "
                    "Default is 0, means no debug dump" , 
                    NULL, NULL);  
  
    ret = parser->Parse(argc, argv, &err_info);//parse the cmd args
    if(ret){
        fprintf(stderr, "Option Parsing Error:%s\n", err_info.c_str());
        exit(-1);
    }

    //check options correct
    
    if(parser->CheckOption("help")){
        std::string option_help;
        option_help = parser->GetOptionsHelp();
        fprintf(stderr, 
        "a StreamSwitch stream source based on the ffmpeg libavformat "
        "which can read the media packet from any media resource. \n"
        "It by now only performs the demuxing from the input resource, "
        "does not include re-encode or any filter, "
        "and publish the result data as a live StreamSwitch stream\n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to terminate this program\n"
        "\n", "ffmpeg_demux_source", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fprintf(stderr, PACKAGE_VERSION"\n");
        exit(0);
    }
    

    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled\n");
            exit(-1);
        }     
    }

}
static void OnSourceErrorFun(int error_code, void *user_data)
{
    exit_code = error_code;    
}

    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    using namespace stream_switch;        
    int ret = 0;
    FFmpegDemuxerSource * source = NULL;
    int pub_queue_size = STSW_PUBLISH_SOCKET_HWM;
    int log_level = 6;
    
    GlobalInit();
    
    //parse the cmd line
    FFmpegArgParser parser;
    ParseArgv(argc, argv, &parser); // parse the cmd line    
    
    
    //
    // init global logger
    log_level = 
        strtol(parser.OptionValue("log-rotate", "6").c_str(), NULL, 0);       
    if(parser.CheckOption(std::string("log-file"))){        
        //init the global logger
        std::string log_file = 
            parser.OptionValue("log-file", "");
        int log_size = 
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        int rotate_num = 
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);      
        
        ret = InitGlobalLogger(log_file, log_size, rotate_num, log_level);
        if(ret){
            fprintf(stderr, "Init Logger failed, exit\n");
            goto exit_1;
        }        
    }else{
        SetLogLevel(log_level);
    }

    /* register all formats and codecs */
    av_register_all();
    avformat_network_init();
   
    //
    //init source
    
    source = FFmpegDemuxerSource::Instance();   
      
    if(parser.CheckOption("queue-size")){
        pub_queue_size = (int)strtol(parser.OptionValue("pub-queue-size", "60").c_str(), NULL, 0);
    }
    ret = source->Init(
        parser.OptionValue("url", ""), 
        parser.OptionValue("stream-name", ""), 
        parser.ffmpeg_options(), 
        (int)strtol(parser.OptionValue("local-gap-max-time", "20").c_str(), NULL, 0), 
        (int)strtol(parser.OptionValue("io_timeout", "10").c_str(), NULL, 0), 
        parser.CheckOption("native-frame-rate");
        (int)strtol(parser.OptionValue("port", "0").c_str(), NULL, 0), 
        pub_queue_size, 
        (int)strtol(parser.OptionValue("debug-flags", "0").c_str(), NULL, 0));
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                    "ffmpeg_demux_srouce init error, exit\n");   
        goto exit_2;       
    }
 
    
    //start ffmpeg_demux_srouce
    ret = source->Start(OnSourceErrorFun, NULL);
    if(ret){
        goto exit_3;
    }

    //drive the proxy heartbeat    
    while(1){
        
        if(isGlobalInterrupt()){
            STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
                      "Receive Terminate Signal, exit\n");  
            ret = exit_code;
            break;
        }
        if(exit_code != 0){
            STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
                      "Source Running Error, exit\n");  
            ret = exit_code;
            break;            
        }

        {
            struct timespec req;
            req.tv_sec = 0;
            req.tv_nsec = 10000000; //10ms             
            nanosleep(&req, NULL);
        }     
              
    }
    
    
    //stop ffmpeg_demux_srouce 
    source->Stop();    
    
exit_3:
    //uninit ffmpeg_demux_srouce
    source->Uninit();
    
exit_2:  

    //uninstance
    FFmpegDemuxerSource::Uninstance();  
  
    //uninit logger
    UninitGlobalLogger();
exit_1:    
    
    //streamswitch library uninit
    GlobalUninit();
    
    return ret;
}


