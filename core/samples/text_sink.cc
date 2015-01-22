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
 * text_sink.cc
 *      a sink sample implementation to output every frames to a text file
 * 
 * author: jamken
 * date: 2015-1-22
**/ 

#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#include <stream_switch.h>

    

//////////////////////////////////////////////////////////////
//global variables




stream_switch::RotateLogger * global_logger;




///////////////////////////////////////////////////////////////
//macro


///////////////////////////////////////////////////////////////
//class

class TextStreamSink:public stream_switch::SinkListener{
  
public:
    TextStreamSink();
    virtual ~TextStreamSink();
    int InitRemote(char * text_file,                 
                   const std::string &source_ip, int source_tcp_port);    
    int InitLocal(char * text_file,  const std::string &stream_name);
    void Uninit();
    int Start();
    void Stop();
    int64_t last_frame_sec()
    {
        return last_frame_sec_;
    }
    virtual void OnLiveMediaFrame(const stream_switch::MediaDataFrame &media_frame);    
       
private: 
    stream_switch::StreamSink sink_;
    std::string text_file_name_;    
    FILE * text_file_;
    int64_t last_frame_sec_;   
    
};


///////////////////////////////////////////////////////////////
//functions
    

int str2int(const std::string &string_temp)
{   
    int int_temp = 0;
	std::stringstream stream(string_temp);
	stream>>int_temp;
    return int_temp;
}


void ParseArgv(int argc, char *argv[], 
               stream_switch::ArgParser *parser)
{
    parser->RegisterBasicOptions();

    //register the default options
    parser->RegisterOption("stream-name", 's', OPTION_FLAG_WITH_ARG, "STREAM",
                   "local stream name, if the user want to connect this sink "
                   "to local stream, this option should be used to set the "
                   "stream name of the stream", NULL, NULL);
    parser->RegisterOption("host", 'H', OPTION_FLAG_WITH_ARG, "HOSTADDR", 
                   "remote host IP address, if the user want to connect this "
                   "sink to a remote stream, this option should be used to "
                   "set the remote host address of this stream", NULL, NULL);                   
    parser->RegisterOption("port", 'p', OPTION_FLAG_WITH_ARG, "PORT", 
                   "remote tcp port, remote host IP address, if the user want "
                   "to connect this sink to a remote stream, this option "
                   "should be used to set the remote tcp port of this stream", 
                   NULL, NULL);
    parser->RegisterOption("log-file", 'l', OPTION_FLAG_WITH_ARG,  "FILE",
                   "log file path for debug", NULL, NULL);   
    parser->RegisterOption("log-size", 'L', OPTION_FLAG_WITH_ARG,  "SIZE",
                   "log file max size in bytes", NULL, NULL);   
    parser->RegisterOption("log-rotate", 'r', OPTION_FLAG_WITH_ARG,  "NUM",
                   "log rotate number, 0 means no rotating", NULL, NULL);       
    parser->RegisterOption("sink-file", 'f', OPTION_FLAG_WITH_ARG,  "FILE", 
                   "the text file path to which this sink dumps the frames. "
                   "This option must be set for text sink", 
                   NULL, NULL); 

  
    parser->Parse(argc, argv);//parse the cmd args

    //check options correct
    
    if(parser->CheckOption(std::string("help"))){
        std::string option_help;
        option_help = parser->GetOptionsHelp();
        fprintf(stderr, 
        "A TEXT stream sink which dumps the receive media frames to a file in text format\n"
        "Usange: %s [options]\n",
        "\n"
        "Option list:\n",
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to end this sink\n"
        "\n", "text_sink", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption(std::string("version"))){
        
        fprintf(stderr, "v0.1.0\n");
        exit(0);
    }
    
    if(!parser->CheckOption(std::string("sink-file"))){
        fprintf(stderr, "sink-file must be set for text sink\n");
        exit(0);        
    }


    if(!parser->CheckOption(std::string("stream-name")) && 
       !parser->CheckOption(std::string("host"))){
        fprintf(stderr, "stream-name or host must be set for text sink\n");
        exit(0);        
    }    
    if(parser->CheckOption(std::string("host"))){
        if(!parser->CheckOption(std::string("port"))){
            fprintf(stderr, "port must be set for a remote source for text sink\n");
            exit(0);
        }
        int port = str2int(parser->OptionValue(std::string("port"), std::string("0")));
        if(port == 0){
            fprintf(stderr, "port is invalid for a remote source\n");
            exit(0);            
        }        
    }
    
     if(parser->CheckOption(std::string("log-file"))){
        if(!parser->CheckOption(std::string("log-size"))){
            fprintf(stderr, "log-size must be set if log-file is enabled for text sink\n");
            exit(0);
        }
        int port = str2int(parser->OptionValue(std::string("log-size"), std::string("0")));
        if(port == 0){
            fprintf(stderr, "log-size is invalid for a remote source\n");
            exit(0);            
        }        
    }   

}

    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    stream_switch::GlobalInit();
    
    //parse the cmd line
    stream_switch::ArgParser parser;
    ParseArgv(argc, argv, &parser); // parse the cmd line
    
    TextStreamSink text_sink;



    stream_switch::GlobalUninit();
}






