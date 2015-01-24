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
 * file_live_source.cc
 *      a live stream source sample which reads from the fix-size media frames 
 * from a given file. When the file is totaly consumed, the source would reset file 
 * to the beginning to continue.
 *      When receive a terminal signal, the source would exit
 * 
 * author: jamken
 * date: 2015-1-17
**/ 



#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#include <stream_switch.h>



///////////////////////////////////////////////////////////////
//Type

class FileLiveSource:public stream_switch::SourceListener{
  
public:
    FileLiveSource();
    virtual ~FileLiveSource();
    int Init(char * text_file, 
             const std::string &stream_name,                       
             int source_tcp_port);    

    void Uninit();
    int Start();
    void Stop();
    void SendNextFrame();

    virtual void OnKeyFrame(void);
    virtual void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic);    
       
private: 
    stream_switch::StreamSink sink_;
    
    std::string src_file_name_;    
    FILE * src_file_;
    int gov;
    
};
    

//////////////////////////////////////////////////////////////
//global variables




stream_switch::RotateLogger * global_logger = NULL;
FileLiveSource * source = NULL;



///////////////////////////////////////////////////////////////
//macro





///////////////////////////////////////////////////////////////
//functions
    
void ParseArgv(int argc, char *argv[], 
               stream_switch::ArgParser *parser)
{
    int ret = 0;
    std::string err_info;
    parser->RegisterBasicOptions();
    parser->RegisterSourceOptions();
    
    parser->RegisterOption("url", 'u', OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG,
                   "FILE_PATH", 
                   "Absolute path of the file which this source read from", NULL, NULL);  

  
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
        "Usange: %s [options]\n",
        "\n"
        "Option list:\n",
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

    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    int ret;
    using namespace stream_switch;
    GlobalInit();
    
    //parse the cmd line
    ArgParser parser;
    ParseArgv(argc, argv, &parser); // parse the cmd line
    
    //
    // init global logger
    if(parser.CheckOption(std::string("log-file"))){
        //init the global logger
        global_logger = new RotateLogger();
        std::string log_file = 
            parser.OptionValue("log-file", "");
        int log_size = 
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        int rotate_num = 
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);      
        int log_level = LOG_LEVEL_DEBUG;
        
        ret = global_logger->Init("file_live_source", 
            log_file, log_size, rotate_num, log_level, false);
        if(ret){
            delete global_logger;
            global_logger = NULL;
            fprintf(stderr, "Init Logger faile\n");
            exit(-1);
        }        
    }
    
    //
    //init source
    source = new FileLiveSource();
    
    
    
    

    //uninit source
    if(source){
        source->Uninit();
        delete source;
        source = NULL;
    }
    
    //uninit logger
    if(global_logger){
        global_logger->Uninit();
        delete global_logger;
        global_logger = NULL;
    }
    GlobalUninit();
}


