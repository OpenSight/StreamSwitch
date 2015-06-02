
#include <stdio.h>
#include <stdlib.h>
#include "stream_switch.h"

void fncheader();
void ParseArgv(int argc, char *argv[], 
               stream_switch::ArgParser *parser)
{
    int ret;
    std::string err_info;
    parser->RegisterBasicOptions();

    //register the default options
    parser->RegisterOption("debug-flags", 'd', 
                    OPTION_FLAG_LONG | OPTION_FLAG_WITH_ARG,  "FLAG", 
                    "debug flag for stream_switch core library. "
                    "Default is 0, means no debug dump" , 
                    NULL, NULL);     
    parser->RegisterOption("port", 'p', 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "PORT", 
                   "RTSP listening port, default is 554", 
                   NULL, NULL);

    parser->RegisterOption("log-type", 't', OPTION_FLAG_WITH_ARG,  "[file|syslog|stderr]",
                   "logger type, default is stderr", NULL, NULL);  
    parser->RegisterOption("log-level", 0, OPTION_FLAG_WITH_ARG,  "NUM",
                   "logger level, from 0 to 6, default is 3(FNC_LOG_INFO)", NULL, NULL);  
                   
    parser->RegisterOption("log-file", 'l', OPTION_FLAG_WITH_ARG,  "PATH",
                   "log file path for debug, only valid for file log type", NULL, NULL);   
    parser->RegisterOption("log-size", 'L', 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,  "SIZE",
                   "log file max size in bytes", NULL, NULL);   
    parser->RegisterOption("log-rotate", 'r', 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,  "NUM",
                   "log rotate number, 0 means no rotating, default is 0", NULL, NULL);       

    parser->RegisterOption("buffer-num", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "NUM", 
                   "the max buffer number in buffer queue, "
                   "each buffer store a (eth)network packet, default is 2048", 
                   NULL, NULL); 

    parser->RegisterOption("buffer-ms", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "NUM", 
                   "the max buffer time(in ms) for in buffer queue, "
                   "default is 2000", 
                   NULL, NULL); 
                   
    parser->RegisterOption("max-conns", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "NUM", 
                   "the max number of the concurrent rtsp connection, "
                   "default is 1000", 
                   NULL, NULL);     

    parser->RegisterOption("max-rate", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "NUM", 
                   "the max scale rate for media replay, "
                   "default is 16, cannot exceed 16", 
                   NULL, NULL);    

    parser->RegisterOption("max-mbps", 0, 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "NUM", 
                   "the max speed (megabits per sec)for media replay, "
                   "0 means not limit, default is 100", 
                   NULL, NULL);  

    parser->RegisterOption("enable-rtcp-heartbeat", 0, 0, NULL, 
                   "enable check for rtcp RR as client heartbeat, "
                   "default is disabled", 
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
        "A live/replay Port server of StreamSwitch for RTSP protocol(RFC 2326)\n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to end this port\n"
        "\n", "stsw_rtsp_port", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fncheader();
        exit(0);
    }


    if(parser->OptionValue("log-type", "stderr") =="file"){
        if(!parser->CheckOption("log-file")){       
            fprintf(stderr, "log-file must be set if log-type is \"file\"\n");
            exit(-1);    
        }
    }
    
    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled for text sink\n");
            exit(-1);
        }
        
        if(strtol(parser->OptionValue("log-size", "0").c_str(), NULL, 0) < 1024){
            fprintf(stderr, "log-size cannot be less than 1K bytes\n");
            exit(-1);
        }
    }
    
}
