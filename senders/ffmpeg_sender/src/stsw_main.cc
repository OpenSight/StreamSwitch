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
 * stsw_main.cc
 *      main entry file for ffmpeg_sender
 * 
 * author: OpenSight Team
 * date: 2015-11-20
**/ 



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stream_switch.h>

extern "C"{
#include <libavformat/avformat.h>
   
}

#include "stsw_ffmpeg_sender_global.h"
#include "stsw_ffmpeg_sender_arg_parser.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_muxer_sender.h"
#include "stsw_cached_segment.h"

///////////////////////////////////////////////////////////////
//Type


//////////////////////////////////////////////////////////////
//global variables


///////////////////////////////////////////////////////////////
//macro

#define FRAME_LOST_CHECK_INTERVAL  1



///////////////////////////////////////////////////////////////
//functions
    
void ParseArgv(int argc, char *argv[], 
               FFmpegSenderArgParser *parser)
{
    int ret = 0;
    std::string err_info;

    parser->RegisterBasicOptions();
    parser->RegisterSenderOptions();
    
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
        "a StreamSwitch stream sender based on the ffmpeg muxing functions, "
        "which reads the media frames from some StreamSwitch source, and "
        "writes them to a ffmpeg muxing context. \n"
        "It only performs the muxing, "
        "does not includes transcoding nor any filter, "
        "and only support read from a live stream\n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to terminate this program\n"
        "\n", "ffmpeg_sender", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fprintf(stderr, PACKAGE_VERSION"\n");
        exit(0);
    }
    
    if(!parser->CheckOption("stream-name") && 
       !parser->CheckOption("host")){
        fprintf(stderr, "stream-name or host must be set for ffmpeg_sender\n");
        exit(-1);        
    }    
    if(parser->CheckOption("host")){
        if(!parser->CheckOption("port")){
            fprintf(stderr, "port must be set for a remote source for ffmpeg_sender\n");
            exit(-1);
        }
    }   
    if(parser->OptionValue("url", "").size() == 0){
        fprintf(stderr, "url cannot be empty string\n");
        exit(-1);        
    }
    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled\n");
            exit(-1);
        }     
    }

}

    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    using namespace stream_switch;        
    int ret = 0;
    FFmpegMuxerSender * sender = NULL;
    uint32_t sub_queue_size = STSW_SUBSCRIBE_SOCKET_HWM;
    int log_level = 6;
    uint64_t max_duration = 0;
    uint64_t max_frame_gap = 0;
    struct timespec cur_ts;    
    struct timespec start_ts;  
    struct timespec last_frame_ts;    
    uint32_t last_frame_num = 0;
    struct timespec lost_check_ts;
    
    GlobalInit();
    
    //parse the cmd line
    FFmpegSenderArgParser parser;
    ParseArgv(argc, argv, &parser); // parse the cmd line    
    
    max_duration = (uint64_t)strtoul(parser.OptionValue("duration", "0").c_str(), NULL, 0);
    max_frame_gap = (uint64_t)strtoul(parser.OptionValue("inter-frame-gap", "20000").c_str(), NULL, 0);
    sub_queue_size = (uint32_t)strtoul(parser.OptionValue("queue-size", "120").c_str(), NULL, 0);      
    
    //
    // init global logger
    log_level = 
        strtol(parser.OptionValue("log-level", "6").c_str(), NULL, 0);       
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
    
    register_cseg();
    //
    //init sender
    
    sender = FFmpegMuxerSender::Instance();     
    ret = sender->Init(
        parser.OptionValue("url", ""), 
        parser.OptionValue("format", ""), 
        parser.ffmpeg_options(), 
        parser.OptionValue("stream-name", ""), 
        parser.OptionValue("host", ""), (int)strtol(parser.OptionValue("port", "0").c_str(), NULL, 0), 
        parser.OptionValue("acodec", ""),
        strtoul(parser.OptionValue("io_timeout", "10000").c_str(), NULL, 0), 
        sub_queue_size, 
        (uint32_t)strtoul(parser.OptionValue("debug-flags", "0").c_str(), NULL, 0));
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                    "ffmpeg_sender init error for url:%s\n", 
                    parser.OptionValue("url", "").c_str());   
        goto exit_2;       
    }
   
    //start sender
    ret = sender->Start();
    if(ret){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                    "ffmpeg_sender start failed for url:%s\n", 
                    parser.OptionValue("url", "").c_str());          
        goto exit_3;
    }
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
               "ffmpeg_sender has started successful for dest URL: %s\n", 
               parser.OptionValue("url", "").c_str());       
    //from here, sender has started successful
    
    clock_gettime(CLOCK_MONOTONIC, &start_ts);  
    cur_ts = last_frame_ts = lost_check_ts = start_ts;
    
    //heartbeat check
    while(1){        
        clock_gettime(CLOCK_MONOTONIC, &cur_ts); 
        
        //check duration
        if(max_duration > 0 ){
            uint64_t cur_dur = 
                (cur_ts.tv_sec - start_ts.tv_sec) * 1000 + 
                (cur_ts.tv_nsec - start_ts.tv_nsec) / 1000000;            
            if(cur_dur >= max_duration){
                ret = 0;
                STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
                          "Duration is over, exit normally\n");   
                break;                
            } 
        }
        
        //check frame gap
        if(max_frame_gap > 0){
            if(sender->frame_num() == last_frame_num){
                uint64_t frame_gap = (cur_ts.tv_sec - last_frame_ts.tv_sec) * 1000 + 
                            (cur_ts.tv_nsec - last_frame_ts.tv_nsec) / 1000000;       
                if(frame_gap > max_frame_gap){
                    ret = FFMPEG_SENDER_ERR_INTER_FRAME_GAP;
                    STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                              "inter-frame-gap is over, exit with error\n");   
                    break;                      
                }
            }else{
                last_frame_ts = cur_ts;
                last_frame_num = sender->frame_num();
            }

        }
        //check sender err
        if((ret = sender->err_code()) != 0){
            if(ret == FFMPEG_SENDER_ERR_EOF){
                ret = 0; //make metadata mismatch exit normally, 
                         //so that the sender can be restart at once
                STDERR_LOG(stream_switch::LOG_LEVEL_INFO,
                           "stream finished, exit normally\n");
            }else{
                STDERR_LOG(stream_switch::LOG_LEVEL_ERR,
                           "sender error with code %d, exit\n", ret);                
            }
            break;
        }
        
        // check frame lost         
        if(cur_ts.tv_sec - lost_check_ts.tv_sec >= 
            FRAME_LOST_CHECK_INTERVAL){
            sender->CheckFrameLost();
            lost_check_ts = cur_ts;
        }
        
        
        // check signal
        if(isGlobalInterrupt()){
            STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
                      "Receive Terminate Signal, exit normally\n");  
            ret = 0;       
/* 
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                printf("signal catchtime is %lld.%06d\n", 
                   (long long)tv.tv_sec, (int)tv.tv_usec);
            }
*/           
            break;
        }
        {
            struct timespec req;
            req.tv_sec = 0;
            req.tv_nsec = 10000000; //10ms             
            nanosleep(&req, NULL);
        }     
              
    }//while(1){ 
    
    
    //stop ffmpeg muxer sender 
    sender->Stop();   
/* 
    STDERR_LOG(stream_switch::LOG_LEVEL_INFO, 
               "ffmpeg_sender has stopped for dest URL: %s\n", 
               parser.OptionValue("url", "").c_str()); 
*/    
exit_3:
    //uninit ffmpeg muxer sender 
    sender->Uninit();    

exit_2:  

    //uninstance
    FFmpegMuxerSender::Uninstance();  
/*    
                  printf("%s:%d\n", 
                   __FILE__, __LINE__); 
*/
    //uninit logger
    UninitGlobalLogger();
exit_1:    
    
    //streamswitch library uninit
    GlobalUninit();
/*
    {
                struct timeval tv;
        gettimeofday(&tv, NULL);
            printf("End time is %lld.%06d\n", 
                   (long long)tv.tv_sec, (int)tv.tv_usec);    
    }
 */
    return ret;
}


