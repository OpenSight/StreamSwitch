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
 * stsw_ffmpeg_muxer.cc
 *      FFmpegMuxer class implementation file, define its methods
 *      FFmpegMuxer is a media file muxer based on ffmpeg libavformat  
 * 
 * author: jamken
 * date: 2015-11-25
**/ 


#include "stsw_ffmpeg_muxer.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_sender_global.h"
#include "parsers/stsw_stream_mux_parser.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <sstream>

extern "C"{

//#include <libavutil/avutil.h>    
#include <libavformat/avformat.h>      
}


FFmpegMuxer::FFmpegMuxer()
:fmt_ctx_(NULL), io_timeout_(0), frame_num_(0)
{
    stream_mux_parsers_.reserve(8); //reserve for 8 streams
    stream_mux_parsers_.clear();
    io_start_ts_.tv_nsec = 0;
    io_start_ts_.tv_sec = 0;
    base_timestamp_.tv_sec = 0;
    base_timestamp_.tv_usec = 0;
    
}
FFmpegMuxer::~FFmpegMuxer()
{
    
}
int FFmpegMuxer::Open(const std::string &dest_url, 
                      const std::string &format,
                      const std::string &ffmpeg_options_str, 
                      const stream_switch::StreamMetadata &metadata, 
                      unsigned long io_timeout)
{
    using namespace stream_switch; 
    int ret = 0;
    AVDictionary *format_opts = NULL;
    const char * format_name = NULL;
    SubStreamMetadataVector::const_iterator meta_it;
    
    
    //printf("ffmpeg_options_str:%s\n", ffmpeg_options_str.c_str());
    //parse ffmpeg_options_str
   
    ret = av_dict_parse_string(&format_opts, 
                               ffmpeg_options_str.c_str(),
                               "=", ",", 0);
    if(ret){
        //log
        STDERR_LOG(LOG_LEVEL_ERR, 
                   "Failed to parse the ffmpeg_options_str:%s to av_dict(ret:%d)\n", 
                   ffmpeg_options_str.c_str(), ret);    
        ret = FFMPEG_SENDER_ERR_GENERAL;                   
        goto err_out1;
    }

    //av_dict_set(&format_opts, "rtsp_transport", "tcp", 0);
    //printf("option dict count:%d\n", av_dict_count(format_opts));

    
    //allocate the format context
    if(format.size() != 0){
        format_name = format.c_str();
    }    
    ret = avformat_alloc_output_context2(&fmt_ctx_, NULL, format_name, dest_url.c_str());    
    if(fmt_ctx_ == NULL){
        //log        
        STDERR_LOG(LOG_LEVEL_ERR, 
                   "Failed to allocate AVFormatContext structure:%s\n", 
                   av_err2str(ret));           
        ret = FFMPEG_SENDER_ERR_GENERAL;
        goto err_out1;        
    }
    //install the io interrupt callback function
    fmt_ctx_->interrupt_callback.callback = FFmpegMuxer::StaticIOInterruptCB;
    fmt_ctx_->interrupt_callback.opaque = this;
    io_timeout_ = io_timeout;


    //setup the parsers
    for(meta_it=metadata.sub_streams.begin();
        meta_it!=metadata.sub_streams.end();
        meta_it++){  
        StreamMuxParser *parser = NewStreamMuxParser(meta_it->codec_name);
        ret = parser->Init(this, (*meta_it), fmt_ctx_);
        if(ret){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "Parser for codec (name:%s) init failed\n", 
                meta_it->codec_name.c_str()); 
            delete parser;
            ret = FFMPEG_SENDER_ERR_GENERAL;
            goto err_out3;
        }
        stream_mux_parsers_.push_back(parser);          
    }
   
    
    // open output file
    if (!(fmt_ctx_->flags & AVFMT_NOFILE)) {
        StartIO();
        ret = avio_open2(&(fmt_ctx_->pb), dest_url.c_str(), AVIO_FLAG_WRITE, 
                         &(fmt_ctx_->interrupt_callback), &format_opts);
        StopIO();
        if (ret < 0) {
            STDERR_LOG(LOG_LEVEL_ERR, 
                   "Could not open output file '%s':%s\n", 
                   dest_url.c_str(), 
                   av_err2str(ret));           
            ret = FFMPEG_SENDER_ERR_GENERAL;
            goto err_out3;             
        }
    }    
    
    ret = avformat_write_header(fmt_ctx_, &format_opts);
    if (ret < 0) {
        STDERR_LOG(LOG_LEVEL_ERR, 
                   "Error occurred when write header to the output file: %s\n", 
                   av_err2str(ret));           
        ret = FFMPEG_SENDER_ERR_GENERAL;
        goto err_out4;         
    }    
    
    if(format_opts != NULL){
        int no_parsed_option_num = av_dict_count(format_opts);
        if(no_parsed_option_num){
            STDERR_LOG(stream_switch::LOG_LEVEL_WARNING,  
            "%d options cannot be parsed by ffmpeg avformat context\n", 
            no_parsed_option_num);            
        }
        //printf("after open, option dict count:%d\n", av_dict_count(format_opts));
        av_dict_free(&format_opts);
        format_opts = NULL;
    }        
    base_timestamp_.tv_sec = 0;
    base_timestamp_.tv_usec = 0;
    frame_num_ = 0;
    return 0;
    
    
err_out4:
    if (fmt_ctx_->oformat && !(fmt_ctx_->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmt_ctx_->pb);     

err_out3:
    
    {
        StreamMuxParserVector::iterator it;
        for(it = stream_mux_parsers_.begin(); 
            it != stream_mux_parsers_.end();
            it++){
            (*it)->Uninit();
            delete (*it);
            (*it) = NULL;
        }
        stream_mux_parsers_.clear();
                
    }
    
//err_out2:
    avformat_free_context(fmt_ctx_);
    fmt_ctx_ = NULL;
    
err_out1:
    if(format_opts != NULL){
        av_dict_free(&format_opts);
        format_opts = NULL;
    }

    return ret;
}


void FFmpegMuxer::Close()
{
 
    if(fmt_ctx_ == NULL){
        return;
    }    
    StartIO();
    //flush all parser
    {
        StreamMuxParserVector::iterator it;
        for(it = stream_mux_parsers_.begin(); 
            it != stream_mux_parsers_.end();
            it++){
            (*it)->Flush();
        }             
    }    
    av_write_trailer(fmt_ctx_);
    StopIO();
    //close ffmpeg format context

    if (fmt_ctx_->oformat && !(fmt_ctx_->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmt_ctx_->pb);  
        
    //uninit all parser
    {
        StreamMuxParserVector::iterator it;
        for(it = stream_mux_parsers_.begin(); 
            it != stream_mux_parsers_.end();
            it++){
            (*it)->Uninit();
            delete (*it);
            (*it) = NULL;
        }
        stream_mux_parsers_.clear();                
    }       
    avformat_free_context(fmt_ctx_);
    fmt_ctx_ = NULL;
}

int FFmpegMuxer::WritePacket(const stream_switch::MediaFrameInfo &frame_info, 
                             const char * frame_data, 
                             size_t frame_size)
{
    int ret = 0;
    AVPacket pkt = { 0 }; // data and size must be 0;
    const stream_switch::MediaFrameInfo * frame_info_p = NULL;
    StreamMuxParser * parser = NULL;
    
    if(fmt_ctx_ == NULL){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "FFmpegMuxer not open\n");
        return -1;
    } 
    if(frame_info.sub_stream_index >= stream_mux_parsers_.size()){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "sub_stream_index is over the number of parser\n");
        return -1;        
    }
    
    //get stream muxer parser
    parser = stream_mux_parsers_[frame_info.sub_stream_index];
    
    for (;;) {
        AVPacket opkt = { 0 };
        av_init_packet(&opkt);
        int ret = parser->Parse(frame_info_p, frame_data, frame_size, &base_timestamp_, &opkt);
        if (frame_info_p != NULL) {
            frame_info_p = NULL;
        }
        if(ret == 0){
            //no pkt need to write, exit
            break;
        }if (ret < 0){
            //some error ocurs in parse
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "Failed to parse the media data from stream %d\n", 
                frame_info.sub_stream_index);                     
            break;
        }
                
#define DUMP_PACKET    
#ifdef DUMP_PACKET
        {
            STDERR_LOG(stream_switch::LOG_LEVEL_DEBUG,  
                        "The following packet will be written to the output file\n");         
                         av_pkt_dump_log2(NULL, AV_LOG_DEBUG, &opkt, 0, 
                         fmt_ctx_->streams[opkt.stream_index]);
                         
        }
#endif             
        StartIO();
        ret = av_interleaved_write_frame(fmt_ctx_, &opkt);
        StopIO();
        if(ret){
            //some error ocurs in parse
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "Failed to write pkt to the output file:%s\n", 
                av_err2str(ret));   
            ret = FFMPEG_SENDER_ERR_IO;            
            break;            
        }
        frame_num_++;
    }    
    return ret;
}

uint32_t FFmpegMuxer::frame_num()
{
    return frame_num_;
}


void FFmpegMuxer::StartIO()
{
    clock_gettime(CLOCK_MONOTONIC, &io_start_ts_);
    
}
void FFmpegMuxer::StopIO()
{
    io_start_ts_.tv_sec = 0;
    io_start_ts_.tv_nsec = 0;
}
int FFmpegMuxer::StaticIOInterruptCB(void* user_data)
{
    FFmpegMuxer *muxer = (FFmpegMuxer *)user_data;
    return muxer->IOInterruptCB();
}

int FFmpegMuxer::IOInterruptCB()
{
    if(io_start_ts_.tv_sec != 0){
        struct timespec cur_ts;
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);
        uint64_t io_dur = (cur_ts.tv_sec - io_start_ts_.tv_sec) * 1000 +
              (cur_ts.tv_nsec - io_start_ts_.tv_nsec) / 1000000;       
        if(io_dur >= (uint64_t)io_timeout_){
            return 1;
        }
    }
    
    return 0;
    
}

static std::string uint2str(unsigned int uint_value)
{
    std::stringstream stream;
    stream<<uint_value;
    return stream.str();
}
void FFmpegMuxer::GetClientInfo(const std::string &dest_url, 
                                const std::string &format,
                                stream_switch::StreamClientInfo *client_info)
{
    const char * protocol_name = NULL; 
    if(client_info == NULL){
        return;
    }
    //get protocol
    protocol_name = avio_find_protocol_name(dest_url.c_str());
    if(protocol_name == NULL){        
        if(format.size() > 0){
            protocol_name = format.c_str();            
        }else{
            AVOutputFormat *oformat;
            oformat = av_guess_format(NULL, dest_url.c_str(), NULL);
            if (oformat!= NULL) {
                protocol_name = oformat->name;
            }
        }//if(format.size() > 0){
    }
    if(protocol_name == NULL){
        protocol_name = "ffmpeg";
    }
    client_info->client_text = protocol_name;
    
    //get ip and port
    {
        char hostname[1024],proto[1024],path[1024];
        int port;
        av_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
                     &port, path, sizeof(path), dest_url.c_str());  
        if(port >0 && port < 65536){
            client_info->client_port = port;
        }
        if (hostname[0] != 0){
            client_info->client_ip = hostname;            
        }
        //the url as the client text
        client_info->client_text = dest_url;
    }
    
    //the pid as the client token
    {
        pid_t pid;
        pid = getpid() ;
        client_info->client_token = uint2str((unsigned)pid);          
    }
}
