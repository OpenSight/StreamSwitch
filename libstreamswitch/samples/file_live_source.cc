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
#include <unistd.h>

#include <stream_switch.h>



///////////////////////////////////////////////////////////////
//Type

class FileLiveSource:public stream_switch::SourceListener{
  
public:
    FileLiveSource();
    virtual ~FileLiveSource();
    int Init(std::string src_file, 
             std::string stream_name,                       
             int source_tcp_port, 
             int frame_size, 
             int fps, 
             int debug_flags);    

    void Uninit();
    int Start();
    void Stop();
    void SendNextFrame();

    virtual void OnKeyFrame(void);
    virtual void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic);    
       
private: 
    stream_switch::StreamSource source_;
    
    std::string src_file_name_;    
    FILE * src_file_;
    int frame_size_;
    uint32_t ssrc_; 
    char * frame_buf_;
    
};
    

//////////////////////////////////////////////////////////////
//global variables




stream_switch::RotateLogger * global_logger = NULL;


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


FileLiveSource::FileLiveSource()
:src_file_(NULL), frame_size_(0), ssrc_(0)
{
    
}
FileLiveSource::~FileLiveSource()
{
    
}

int FileLiveSource::Init(std::string src_file, 
                         std::string stream_name,                       
                         int source_tcp_port, 
                         int frame_size, 
                         int fps,
                         int debug_flags)
{
    using namespace stream_switch;         
    int ret;
    std::string err_info;
    StreamMetadata metadata;
    SubStreamMetadata sub_metadata;
    
    if(frame_size <= 0){
        fprintf(stderr, "frame_size cannot be zero");
        return -1;
    }
    
    //open file
    src_file_ = fopen(src_file.c_str(), "r");
    if(src_file_ == NULL){
        perror("Open Text File Failed");
        return -1;
    }
    src_file_name_ = src_file;
    
    
    //init source
    ret = source_.Init(stream_name, source_tcp_port, this, debug_flags, &err_info);
    if(ret){
        fprintf(stderr, "Init stream source error: %s\n", err_info.c_str());
        fclose(src_file_);
        src_file_ = NULL;
        src_file_name_.clear();
        return -1;
    }
    
    //setup metadata
    metadata.bps = 0;
    metadata.play_type = Stream_PLAY_TYPE_LIVE;
    metadata.source_proto = "FileSystem";
    srand(time(NULL));
    ssrc_ = (uint32_t)(rand() % 0xffffffff); 
    metadata.ssrc = ssrc_;
    sub_metadata.codec_name = "Private";
    sub_metadata.media_type = SUB_STREAM_MEIDA_TYPE_VIDEO;
    sub_metadata.sub_stream_index = 0;
    sub_metadata.direction = SUB_STREAM_DIRECTION_OUTBOUND;
    sub_metadata.media_param.video.height = 1080;
    sub_metadata.media_param.video.width = 1920;
    sub_metadata.media_param.video.fps = fps;
    sub_metadata.media_param.video.gov = fps;
    metadata.sub_streams.push_back(sub_metadata);
    source_.set_stream_meta(metadata);    
    
    frame_size_ = frame_size;
    frame_buf_ = new char[frame_size_];
    
    ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
              "FileLiveSource Init successful (file:%s, stream:%s)", 
              src_file.c_str(), stream_name.c_str());    


    return 0;
}

void FileLiveSource::Uninit()
{
    delete [] frame_buf_;
    frame_size_ = 0;
    source_.Uninit();
    fclose(src_file_);
    src_file_ = NULL;
    src_file_name_.clear();    
}


int FileLiveSource::Start()
{
    int ret = 0;
    std::string err_info;
    
    source_.set_stream_state(stream_switch::SOURCE_STREAM_STATE_OK);
    
    ret = source_.Start(&err_info);
    if(ret){
        fprintf(stderr, "Start stream source error: %s\n", err_info.c_str());        
    }
    
    fprintf(stdout, "FileLiveSource Started\n");
    ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
              "FileLiveSource Started");      
    
    
    return ret;
}
void FileLiveSource::Stop()
{
    source_.Stop();
}



void FileLiveSource::SendNextFrame()
{
    stream_switch::MediaFrameInfo frame;    
    int ret;
    std::string err_info;
    
    
    
    ret = fread(frame_buf_, 1, frame_size_, src_file_);
    if(ret == 0){
        fseek(src_file_, 0, SEEK_SET);
        return;
    }else if (ret < frame_size_){
        fseek(src_file_, 0, SEEK_SET);
    }

    frame.frame_type = stream_switch::MEDIA_FRAME_TYPE_DATA_FRAME;
    frame.sub_stream_index = 0;
    frame.ssrc = ssrc_;

    gettimeofday(&(frame.timestamp), NULL);
    
    ret = source_.SendLiveMediaFrame(frame, frame_buf_, frame_size_, &err_info);
    
}


void FileLiveSource::OnKeyFrame(void)
{
    //no key frame
}
void FileLiveSource::OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic)
{
    //use the default statistic info
}


    
///////////////////////////////////////////////////////////////
//main entry    
int main(int argc, char *argv[])
{
    int ret = 0;
    using namespace stream_switch;
    FileLiveSource source;
    struct timeval next_send_tv;
    int fps, frame_dur;
    
    
    
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
            ret = -1;
            goto exit_1;
        }        
    }
    
    //
    //init source
    
    fps = (int)strtol(parser.OptionValue("fps", "25").c_str(), NULL, 0);
    frame_dur = 1000000 / fps;
       
    
    ret = source.Init(
        parser.OptionValue("url", ""), 
        parser.OptionValue("stream-name", ""), 
        (int)strtol(parser.OptionValue("port", "0").c_str(), NULL, 0), 
        (int)strtol(parser.OptionValue("frame-size", "1024").c_str(), NULL, 0), 
        fps, 
        (int)strtol(parser.OptionValue("debug-flags", "0").c_str(), NULL, 0));
    if(ret){
        ret = -1;
        goto exit_2;       
    }
    

    
    ret = source.Start();
    if(ret){
        ret = -1;
        goto exit_3;
    }
    gettimeofday(&next_send_tv, NULL);
    
    while(1){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        long long waittime;
        
        if(isGlobalInterrupt()){
            fprintf(stderr, "Receive Terminate Signal, exit\n");
            ROTATE_LOG(global_logger, stream_switch::LOG_LEVEL_INFO, 
                      "Receive Terminate Signal, exit\n");  
            ret = 0;    
            break;
        }        
        
        
        
        //check need to send next frame;
        if( (tv.tv_sec > next_send_tv.tv_sec)
            || (tv.tv_sec == next_send_tv.tv_sec 
                && tv.tv_usec >= next_send_tv.tv_usec)){
            //calculate next send time
            next_send_tv.tv_usec += frame_dur;
            if(next_send_tv.tv_usec > 1000000){
                next_send_tv.tv_sec += next_send_tv.tv_usec / 1000000;
                next_send_tv.tv_usec =  next_send_tv.tv_usec % 1000000;
            }
            source.SendNextFrame();
        }
        
        waittime = (next_send_tv.tv_sec - tv.tv_sec) * 1000000 
                   + (next_send_tv.tv_usec - tv.tv_usec);
        if(waittime < 0) {
            waittime = 0;
        }else if (waittime > 100000){
            waittime = 100000; //at most 100 ms
        }
        
        usleep(waittime);                
    }
    
 
    source.Stop();    
    
exit_3:
    //uninit source
    source.Uninit();
    
exit_2:    
    //uninit logger
    if(global_logger){
        global_logger->Uninit();
        delete global_logger;
        global_logger = NULL;
    }
exit_1:    
    
    GlobalUninit();
    
    return ret;
}


