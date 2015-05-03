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
 * api_test_sink.cc
 *      a sink sample which is used to test the core library api
 * 
 * author: jamken
 * date: 2015-2-11
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

class ApiTestSinkListener:public stream_switch::SinkListener{
  
public:

    virtual void OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size
    
                                 );    
       
  
        
};


///////////////////////////////////////////////////////////////
//functions


static std::string int2str(int int_value)
{
    std::stringstream stream;
    stream<<int_value;
    return stream.str();
}

void ApiTestSinkListener::OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                           const char * frame_data, 
                                           size_t frame_size)
{
    //just ignore the receive frame
}

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
    parser->RegisterOption("stream-name", 's', OPTION_FLAG_WITH_ARG, "STREAM",
                   "local stream name, if the user want to connect this sink "
                   "to local stream, this option should be used to set the "
                   "stream name of the stream", NULL, NULL);
    parser->RegisterOption("host", 'H', OPTION_FLAG_WITH_ARG, "HOSTADDR", 
                   "remote host IP address, if the user want to connect this "
                   "sink to a remote stream, this option should be used to "
                   "set the remote host address of this stream", NULL, NULL);                   
    parser->RegisterOption("port", 'p', 
                   OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG, "PORT", 
                   "remote tcp port, remote host IP address, if the user want "
                   "to connect this sink to a remote stream, this option "
                   "should be used to set the remote tcp port of this stream", 
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
        "A API test sink which is just to test the stream_switch sink API \n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "\n", "api_test_sink", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fprintf(stderr, "v0.1.0\n");
        exit(0);
    }

    if(!parser->CheckOption("stream-name") && 
       !parser->CheckOption("host")){
        fprintf(stderr, "stream-name or host must be set for text sink\n");
        exit(-1);        
    }    
    if(parser->CheckOption("host")){
        if(!parser->CheckOption("port")){
            fprintf(stderr, "port must be set for a remote source for text sink\n");
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
    StreamSink test_sink;
    ArgParser parser;
    StreamClientInfo client_info;
    ApiTestSinkListener listener;
    std::string err_info;
    StreamMetadata metadata;
    MediaStatisticInfo statistic;
    uint32_t client_num;
    StreamClientList client_list;
    
    
   
    GlobalInit();
    
    //parse the cmd line

    ParseArgv(argc, argv, &parser); // parse the cmd line    
    
    //setup client info
    client_info.client_protocol = "NULL";
    client_info.client_text = "api_test_sink which which is just to test the stream_switch sink API";
    //use ramdon number as token
    srand(time(NULL));
    client_info.client_token = int2str(rand() % 0xffffff);   
     
    

    
    if(parser.CheckOption("stream-name")){
        
        ret = test_sink.InitLocal(parser.OptionValue("stream-name", "default"), 
            client_info, &listener, 
            strtol(parser.OptionValue("debug-flags", "0").c_str(), NULL, 0), 
            &err_info);

        
        
    }else if(parser.CheckOption("host")){
        
        ret = test_sink.InitRemote(parser.OptionValue("host", ""), 
            (int)strtol(parser.OptionValue("port", "0").c_str(), NULL, 0),
            client_info, &listener, 
            strtol(parser.OptionValue("debug-flags", "0").c_str(), NULL, 0),  
            &err_info);
  
    }
    if(ret){
        fprintf(stderr, "Init Sink Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_2;

    }
    
    //test each api of sink
    ret = test_sink.UpdateStreamMetaData(5000, &metadata, &err_info);
    if(ret){
        fprintf(stderr, "Call UpdateStreamMetaData() Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_3;

    }    

    ret = test_sink.SourceStatistic(5000, &statistic, &err_info);
    if(ret){
        fprintf(stderr, "Call SourceStatistic() Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_3;

    }
    
    ret = test_sink.ClientList(5000, 0, STSW_MAX_CLIENT_NUM, 
                               &client_num, &client_list, &err_info);
    if(ret){
        fprintf(stderr, "Call ClientList() Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_3;

    }    
    
    
    ret = test_sink.Start(&err_info);
    if(ret){
        fprintf(stderr, "call Start() Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_3;                
    }

    ret = test_sink.KeyFrame(5000, &err_info);
    if(ret){
        fprintf(stderr, "SourceStatistic Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_4;        
    }
    
    fprintf(stderr, "sleep 10.......\n");
    usleep(10000000); //10 sec
    
    
    test_sink.ReceiverStatistic(&statistic);    
    
    ret = test_sink.ClientList(5000, 0, STSW_MAX_CLIENT_NUM, 
                               &client_num, &client_list, &err_info);
    if(ret){
        fprintf(stderr, "Call ClientList() Failed (%d):%s\n", ret, err_info.c_str());
        ret = -1;
        goto exit_4;

    }    
       

exit_4:
    
    test_sink.Stop();


exit_3:    
    test_sink.Uninit();

exit_2:
    GlobalUninit();

    
    return ret;
}






