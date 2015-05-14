/*
 * stsw_rtmp_main.cc
 *
 *  Created on: Apr 21, 2015
 *      Author: hyt
 */


#include "stsw_rtmp_source.h"

stream_switch::RotateLogger * logger = NULL;


void ParseArgv(int argc, char *argv[],
               stream_switch::ArgParser *parser)
{
    int ret = 0;
    std::string err_info;
    parser->RegisterBasicOptions();
    parser->RegisterSourceOptions();

    parser->RegisterOption("frame-size", 'F',  OPTION_FLAG_WITH_ARG,
                   "SIZE",
                   "Frame size to send out", NULL, NULL);
    parser->RegisterOption("fps", 'f',  OPTION_FLAG_WITH_ARG,
                   "NUM",
                   "Frames per secode to send", NULL, NULL);

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
        "A live stream source which reads the media frames from a specified file in cycl\n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to terminate this source\n"
        "\n", "file_live_source", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){

        fprintf(stderr, "v0.1.0\n");
        exit(0);
    }


    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled\n");
            exit(-1);
        }
    }


}



static void SignalHandler (int signal_num)
{
    RtmpClientSource * app = NULL;

    stream_switch::SetGolbalInterrupt(true);

    app = RtmpClientSource::Instance();

    app->SetQuit();
}


int main(int argc, char *argv[]) {

    int ret = 0;
    RtmpClientSource* rtmpClient;

    stream_switch::GlobalInit();

    //parse the cmd line
    stream_switch::ArgParser parser;
    ParseArgv(argc, argv, &parser); // parse the cmd line

    //
    // init global logger
    if(parser.CheckOption(std::string("log-file"))){
        //init the global logger
        logger = new stream_switch::RotateLogger();
        std::string log_file =
            parser.OptionValue("log-file", "");
        int log_size =
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        int rotate_num =
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);
        int log_level = stream_switch::LOG_LEVEL_DEBUG;

        ret = logger->Init("rtmp_source",
            log_file, log_size, rotate_num, log_level, true);
        if(ret){
            delete logger;
            logger = NULL;
            fprintf(stderr, "Init Logger faile\n");
            ret = -1;
            goto exit_1;
        }
    }

    // setup librtmp logger
    RTMP_LogSetLevel(RTMP_LOGDEBUG);

    // "rtmp://192.168.1.180:1935/vod/wow.flv"
    rtmpClient = RtmpClientSource::Instance();

    stream_switch::SetIntSigHandler(SignalHandler);

    if (rtmpClient->Connect(std::string(parser.OptionValue("url", ""))) != 0) {
        return -1;
    }


    return 0;

exit_2:
    if (logger) {
        logger->Uninit();
        delete logger;
        logger = NULL;
    }


exit_1:
    stream_switch::GlobalUninit();
    return ret;

}

