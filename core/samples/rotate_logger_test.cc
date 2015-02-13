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
 * rotate_logger_test.cc
 *      a sample to test the functionality of rotate logger
 * 
 * author: jamken
 * date: 2015-2-13
**/ 

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>

#include <stream_switch.h>

    

//////////////////////////////////////////////////////////////
//global variables









///////////////////////////////////////////////////////////////
//macro


///////////////////////////////////////////////////////////////
//type class




///////////////////////////////////////////////////////////////
//functions
    



void ParseArgv(int argc, char *argv[], 
               stream_switch::ArgParser *parser)
{
    int ret;
    std::string err_info;
    parser->RegisterBasicOptions();

    //register the default options
    parser->RegisterOption("log-file", 'l', 
                    OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG,  "FILE",
                   "log file path for debug", NULL, NULL);   
    parser->RegisterOption("log-size", 'L', 
                   OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,  "SIZE",
                   "log file max size in bytes", NULL, NULL);   
    parser->RegisterOption("log-rotate", 'r', 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,  "NUM",
                   "log rotate number, Default is 0,  means no rotating", NULL, NULL);       
    parser->RegisterOption("stderr", 0, 0,  NULL, 
                   "Redirect stderr to rotate logger", 
                   NULL, NULL);                    
     parser->RegisterOption("repeat", 'R', 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "NUM", 
                   "how many times the records to be written to the log file repeatly, "
                   "Default is 0, mean no repeat, only one record would be written" , 
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
        "a test sample to test the functionality of rotate logger\n"
        "Usange: %s [options]  \"record_string\"\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "\n", "rotate_logger_test", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fprintf(stderr, "v0.1.0\n");
        exit(0);
    }


    
    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled for text sink\n");
            exit(-1);
        }
    }
    if(parser->non_options().size() != 1 ){
        fprintf(stderr, "record string must be given\n");
        exit(-1);        
    }   

}

    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    int ret = 0;
    using namespace stream_switch;    
    stream_switch::RotateLogger * test_logger;
    std::string  record;
    ArgParser parser;
    int repeat = 0;
    
   
    GlobalInit();    
    
    //init the global logger
    test_logger = new RotateLogger();
    
    
    //parse the cmd line
    ParseArgv(argc, argv, &parser); // parse the cmd line    

    //init logger
    {
        std::string log_file = 
            parser.OptionValue("log-file", "");
        int log_size = 
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        int rotate_num = 
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);      
        int log_level = LOG_LEVEL_DEBUG;
        
        bool redirect_stderr = parser.CheckOption("stderr");
        
        ret = test_logger->Init("file_live_source", 
            log_file, log_size, rotate_num, log_level, redirect_stderr);
        if(ret){

            fprintf(stderr, "Init Logger faile\n");
            ret = -1;
            goto exit_2;
        }        

    }
        
    record = parser.non_options()[0];
    repeat = strtol(parser.OptionValue("repeat", "0").c_str(), NULL, 0);   
    
    
    fprintf(stderr, "Write the record to logger: %s\n", record.c_str());
    ROTATE_LOG(test_logger, stream_switch::LOG_LEVEL_INFO, 
               record.c_str());
    
    
    for(int i=0; i<repeat; i++){
        fprintf(stderr, "Write the record to logger (Repeat %d): %s\n", 
                i + 1, record.c_str());
        ROTATE_LOG(test_logger, stream_switch::LOG_LEVEL_INFO, 
                record.c_str());
        
        usleep(1000); //1ms
        
    }


    
    test_logger->Uninit();
    
exit_2:

    if(test_logger != NULL){
        delete test_logger;
        test_logger = NULL;
    }

    GlobalUninit();

    
    return ret;
}






