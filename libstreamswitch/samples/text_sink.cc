/**
 * This file is part of libstreamswtich, which belongs to StreamSwitch
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
 * text_sink.cc
 *      a sink sample implementation to output every frames to a text file
 * 
 * author: jamken
 * date: 2015-1-22
**/ 

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stream_switch.h>

    

//////////////////////////////////////////////////////////////
//global variables




class StreamMetadata;
stream_switch::RotateLogger * global_logger;




///////////////////////////////////////////////////////////////
//macro


///////////////////////////////////////////////////////////////
//type class

class TextStreamSink:public stream_switch::SinkListener{
  
public:
    TextStreamSink();
    virtual ~TextStreamSink();
    int InitRemote(std::string info_file,       
                   std::string source_ip, int source_tcp_port, 
                   int queue_size, 
                   int debug_flags);    
    int InitLocal(std::string info_file, 
                  std::string stream_name, 
                  int queue_size,
                  int debug_flags);
    void Uninit();
    int Start(int timeout);
    void Stop();
    int64_t last_frame_rec_sec()
    {
        return last_frame_rec_sec_;
    }
    virtual void OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size);    
                                  
    virtual void OnMetadataMismatch(uint32_t mismatch_ssrc);    
    
    bool is_err()
    {
        return is_err_;
    }
       
private: 
    stream_switch::StreamSink sink_;
    std::string info_file_name_;    
    FILE * info_file_;
#define MAX_SUB_STREAM_NUMF 64
    FILE * data_files_[MAX_SUB_STREAM_NUMF];
    
    int64_t last_frame_rec_sec_;   
    stream_switch::StreamClientInfo client_info;
    bool is_err_;
  
        
};


///////////////////////////////////////////////////////////////
//functions
    
    
static void Digit2Char(char *dst, uint8_t src)
{
    if (src < 10) {
        *dst = '0' + src;
    } else {
        *dst = 'A' + src - 10;
    }
}    
    
static std::string int2str(int int_value)
{
    std::stringstream stream;
    stream<<int_value;
    return stream.str();
}

static int str2int(std::string str_value)
{
    return strtol(str_value.c_str(), NULL, 0);
}


TextStreamSink::TextStreamSink()
:info_file_(NULL), last_frame_rec_sec_(0), is_err_(false)
{
    client_info.client_protocol = "text_dump";
    client_info.client_text = "text_sink which dumps media frames to a text file";
    //use ramdon number as token
    srand(time(NULL));
    client_info.client_token = int2str(rand() % 0xffffff);   
    for(int i=0;i<MAX_SUB_STREAM_NUMF;i++){
        data_files_[i] = NULL;
    }
}

TextStreamSink::~TextStreamSink()
{
    
}

int TextStreamSink::InitRemote(std::string info_file,             
                   std::string source_ip, int source_tcp_port, 
                   int queue_size,
                   int debug_flags)
{
    int ret;
    std::string err_info;
    
    //open file
    if(info_file.size() != 0){
        info_file_ = fopen(info_file.c_str(), "w");
        if(info_file_ == NULL){
            perror("Open Text File Failed");
            return -1;
        }
        info_file_name_ = info_file;        
    }
  
    
    //init sink
    ret = sink_.InitRemote(source_ip, source_tcp_port, client_info, 
                           queue_size, 
                           this, 
                           debug_flags, &err_info);
    if(ret){
        fprintf(stderr, "Init stream sink error: %s\n", err_info.c_str());
        fclose(info_file_);
        info_file_ = NULL;
        ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_ERR, 
                  "TextStreamSink Init failed: %s", err_info.c_str());           
        return -1;
    }
    
    is_err_ = false;
    
    ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
              "TextStreamSink Init successful (info-file:%s, source: %s:%d)", 
              info_file.c_str(), source_ip.c_str(), source_tcp_port);

    return 0;    
    
} 
int TextStreamSink::InitLocal(std::string info_file, 
                              std::string stream_name, 
                              int queue_size,
                              int debug_flags)
{
    int ret;
    std::string err_info;
    
    //open file
    //open file
    if(info_file.size() != 0){
        info_file_ = fopen(info_file.c_str(), "w");
        if(info_file_ == NULL){
            perror("Open Text File Failed");
            return -1;
        }
        info_file_name_ = info_file;        
    }
    
    
    //init sink
    ret = sink_.InitLocal(stream_name, client_info, 
                          queue_size, 
                          this, 
                          debug_flags, &err_info);
    if(ret){
        fprintf(stderr, "Init stream sink error: %s\n", err_info.c_str());
        fclose(info_file_);
        info_file_ = NULL;
        ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_ERR, 
                  "TextStreamSink Init failed: %s\n", err_info.c_str());           
        info_file_name_.clear();
        return -1;
    }
    
    last_frame_rec_sec_ = 0;
    is_err_ = false;
    
    ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
              "TextStreamSink Init successful (info-file:%s, source:%s)", 
              info_file.c_str(),  stream_name.c_str());    

    return 0;        
}

void TextStreamSink::Uninit()
{
    sink_.Uninit();
    if(info_file_ != NULL){
        fclose(info_file_);
        info_file_ = NULL;
    
    }
    info_file_name_.clear();

}


int TextStreamSink::Start(int timeout)
{
    int ret;
    std::string err_info;
    stream_switch::StreamMetadata metadata;
    stream_switch::SubStreamMetadataVector::iterator meta_it;
    
    
    //Get the metadata of the stream from the source
    ret = sink_.UpdateStreamMetaData(timeout, &metadata, &err_info);
    if(ret){
        fprintf(stderr, "TextStreamSink update metaData failed: %s\n", err_info.c_str());
        ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_ERR, 
                  "TextStreamSink update metaData failed: %s\n", err_info.c_str());   
        return -1;
    }
    
    //
    //do something with the new metadata
    //
    //dump metadat to info file
    if(info_file_){
        char *tmp_buf = new char[2048];
        
        fprintf(info_file_, 
                "Metadata - play_type:%d, source_proto:%s, ssrc:0x%x, stream_len: %f"
                ", bps:%u, sub stream number:%d\n", 
                (int)metadata.play_type, 
                metadata.source_proto.c_str(), 
                metadata.ssrc, 
                metadata.stream_len, 
                metadata.bps, 
                (int)metadata.sub_streams.size());

        for(meta_it=metadata.sub_streams.begin();
            meta_it!=metadata.sub_streams.end();
            meta_it++){
                
                
            std::string data_file_name;
           
            //open the data file from stream
            data_file_name += info_file_name_;
            data_file_name += ".";
            data_file_name += ('0' + meta_it->sub_stream_index);
            data_files_[meta_it->sub_stream_index] = 
                fopen(data_file_name.c_str(), "wb");
            if(data_files_[meta_it->sub_stream_index] == NULL){
                perror("Open data File Failed");
                for(int i=0;i<MAX_SUB_STREAM_NUMF;i++){
                    if(data_files_[i] != NULL){
                        fclose(data_files_[i]);
                        data_files_[i] = NULL;
                    }
                    
                }
                delete[] tmp_buf;
                return -1;
            }            
        
            
            int i = 0;
            if(meta_it->extra_data.size() == 0){
                strcpy(tmp_buf, "Empty");
            }else if(meta_it->extra_data.size() > 1024){
                strcpy(tmp_buf, "> 1024 bytes");
            }else{
                memset(tmp_buf, 0, 2048);
                for(i=0;i < (int)meta_it->extra_data.size(); i++){
                    Digit2Char(tmp_buf + 2 * i, 
                               (((unsigned char*)meta_it->extra_data.data())[i] >> 4) & 0xF);
                    Digit2Char(tmp_buf + 2 * i + 1, 
                               ((unsigned char*)meta_it->extra_data.data())[i] & 0xF);                    
                }
            }

            
            fprintf(info_file_,  
                    "Stream %d - data_file:%s, media_type:%d, codec_name:%s, direction:%d"
                    ", extra_data:%s\n", 
                    (int)meta_it->sub_stream_index,
                    data_file_name.c_str(), 
                    (int)meta_it->media_type, 
                    meta_it->codec_name.c_str(), 
                    (int)meta_it->direction, 
                    tmp_buf);
                  
        }
        delete[] tmp_buf;
    }//if(info_file_){
    
    last_frame_rec_sec_ = time(NULL); 
    is_err_ = false;
    
    
    // start to receive the media frames
    ret = sink_.Start(&err_info);
    if(ret){
        fprintf(stderr, "TextStreamSink start failed: %s\n", err_info.c_str());
        ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_ERR, 
                  "TextStreamSink start failed: %s\n", err_info.c_str());   
        return -1;
    }
    
    ret = sink_.KeyFrame(timeout, &err_info);
    if(ret){
        fprintf(stderr, "Request key frame failed: %s\n", err_info.c_str());
        ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_WARNING, 
                  "Request key frame failed: %s\n", err_info.c_str());        
        sink_.Stop();
        return -1;
    }


    fprintf(stdout, "TextStreamSink Started\n");
    ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
              "TextStreamSink Started");  

    return 0;
    
}

void TextStreamSink::Stop()
{
    sink_.Stop();
    ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
              "TextStreamSink Stopped");    
    for(int i=0;i<MAX_SUB_STREAM_NUMF;i++){
        if(data_files_[i] != NULL){
            fclose(data_files_[i]);
            data_files_[i] = NULL;
        }
    }               

}


void TextStreamSink::OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                      const char * frame_data, 
                                      size_t frame_size)
{
    
    if(info_file_){
        char tmp_buf[10];
        long pos = 0;
        
        if(data_files_[frame_info.sub_stream_index]){
            pos = ftell(data_files_[frame_info.sub_stream_index]);
        }
        
        fprintf(info_file_, 
                "index:%d, type:%d, time:%lld.%03d, ssrc:0x%x, size: %d, data_file pos:%ld\n", 
                (int)frame_info.sub_stream_index, 
                frame_info.frame_type, 
                (long long)frame_info.timestamp.tv_sec, 
                (int)(frame_info.timestamp.tv_usec/1000), 
                (unsigned)frame_info.ssrc, 
                (int)(frame_size),
                pos);
#if 0                
        int i = 0;
        for(i=0;i < (int)frame_size; i++){
            if(i % 32 == 0){
                fprintf(info_file_, "\n");                
            }
            
            //snprintf(tmp_buf, 10, "%d ", (int)0);
            fprintf(info_file_, "%02hhx ", frame_data[i]);
            //fprintf(text_file_, "%02hhx ", (char)0);
        }
        fprintf(info_file_, "\n\n"); 
#endif    
        if(data_files_[frame_info.sub_stream_index]){
            fwrite(frame_data, 1, frame_size, data_files_[frame_info.sub_stream_index]);
        }
    }

    
    last_frame_rec_sec_ = time(NULL);    
 
}

void TextStreamSink::OnMetadataMismatch(uint32_t mismatch_ssrc)
{
    fprintf(stderr, "TextStreamSink::OnMetadataMismatch() is called with mismatch_ssrc(%x)", 
            mismatch_ssrc);    
    is_err_ = true;
}



void ParseArgv(int argc, char *argv[], 
               stream_switch::ArgParser *parser)
{
    int ret;
    std::string err_info;
    parser->RegisterBasicOptions();
    parser->RegisterSenderOptions();
    
    parser->UnregisterOption("url");
    parser->RegisterOption("url", 'u', OPTION_FLAG_WITH_ARG,
                   "FILE", 
                   "the text file path to which this sink dumps the frames info. "
                   "If not given, no file is generated", NULL, NULL);  
    parser->UnregisterOption("format");
    parser->RegisterOption("format", 'f', OPTION_FLAG_WITH_ARG,
                   "NAME", 
                   "the dest file format, this option make no sense for text_sink", 
                   NULL, NULL);    

    parser->RegisterOption("queue-size", 'q', 
                    OPTION_FLAG_WITH_ARG | OPTION_FLAG_LONG,
                    "NUM",
                    "the size of the message queue for Subscriber, 0 means no limit."
                    "Default is an internal value determined when compiling", NULL, NULL);      

    
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
        "A TEXT stream sink which dumps the receive media frames to a file in text format\n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to end this sink\n"
        "\n", "text_sink", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fprintf(stderr, PACKAGE_VERSION"\n");
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
    
    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled for text sink\n");
            exit(-1);
        }
    }
/*    
    if(!parser->CheckOption("sink-file")){
        fprintf(stderr, "sink-file must be given\n");
        exit(-1);        
    }
*/
    

}

    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    int ret = 0;
    int queue_size = STSW_SUBSCRIBE_SOCKET_HWM;
    using namespace stream_switch;    
 
    TextStreamSink text_sink;
    ArgParser parser;
   
    GlobalInit();
    
    //parse the cmd line

    ParseArgv(argc, argv, &parser); // parse the cmd line    
    
    if(parser.CheckOption(std::string("log-file"))){
        //init the global logger
        global_logger = new RotateLogger();
        std::string log_file = 
            parser.OptionValue("log-file", "");
        int log_size = (int)
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        int rotate_num = (int)
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);      
        int log_level = (int)
            strtol(parser.OptionValue("log-level", "6").c_str(), NULL, 0);      

        
        ret = global_logger->Init("file_live_source", 
            log_file, log_size, rotate_num, log_level, false);
        if(ret){
            delete global_logger;
            global_logger = NULL;
            fprintf(stderr, "Init Logger faile\n");
            ret = -1;
            goto exit_2;
        }        
    }    
    

    if(parser.CheckOption("queue-size")){
        queue_size = (int)strtol(parser.OptionValue("queue-size", "120").c_str(), NULL, 0);
    }

     
    

    
    if(parser.CheckOption("stream-name")){
        ret = text_sink.InitLocal(
            parser.OptionValue("url", ""),
            parser.OptionValue("stream-name", "default"), 
            queue_size,
            str2int(parser.OptionValue("debug-flags", "0")));
        
        
    }else if(parser.CheckOption("host")){
        ret = text_sink.InitRemote(
            parser.OptionValue("url", ""),        
            parser.OptionValue("host", ""),
            str2int(parser.OptionValue("port", "0")), 
            queue_size, 
            str2int(parser.OptionValue("debug-flags", "0")));
        
    }
    if(ret){    
        ret = -1;
        goto exit_3;

    }

    
    
    ret = text_sink.Start(5000);//5 seconds to wait for metadata
    if(ret){        
        ret = -1;
        goto exit_4;
    }    


#if 1   
    
    while(1){
        struct timeval tv;
        gettimeofday(&tv, NULL);
#define NO_DATA_INTERVAL     10    
    
        if(tv.tv_sec - text_sink.last_frame_rec_sec() >= NO_DATA_INTERVAL){
            fprintf(stderr, "No frame receive for %d sec, exit\n", NO_DATA_INTERVAL);
            ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_ERR, 
                      "No frame receive for %d sec, exit\n", NO_DATA_INTERVAL);  
            ret = -1;
            break;
        }
        if(text_sink.is_err()){
            fprintf(stderr, "text_sink is error, exit\n");
            ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_ERR, 
                       "text_sink is error, exit\n");  
            ret = -1;
            break;            
        }
      
        if(isGlobalInterrupt()){
            fprintf(stderr, "Receive Terminate Signal, exit\n");
            ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
                      "Receive Terminate Signal, exit\n");  
            /*
            printf("signal catchtime is %lld.%06d\n", 
                   (long long)tv.tv_sec, (int)tv.tv_usec);
            */              
                      
            ret = 0;    
            break;
        }
        {
            struct timespec req;
            req.tv_sec = 0;
            req.tv_nsec = 100 * 1000 * 1000;      //100ms      
            nanosleep(&req, NULL);

        }          
 
        
    }    
    
#endif
    
    
    text_sink.Stop();


exit_4:    
    text_sink.Uninit();

exit_3: 
    
    if(global_logger != NULL){
        global_logger->Uninit();
        delete global_logger;
        global_logger = NULL;
    }

exit_2:
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






