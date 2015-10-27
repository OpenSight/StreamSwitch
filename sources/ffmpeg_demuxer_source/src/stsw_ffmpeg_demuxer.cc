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
 * stsw_ffmpeg_demuxer.cc
 *      FfmpegDemuxer class implementation file, define its methods
 *      FFmpegDemuxer is a media file demuxer based on ffmpeg libavformat 
 * 
 * author: jamken
 * date: 2015-10-23
**/ 


#include "stsw_ffmpeg_demuxer.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_source_global.h"


extern "C"{

#include <libavutil/avutil.h>    
#include <libavformat/avformat.h>      
}


FFmpegDemuxer::FFmpegDemuxer()
:io_enabled_(true), fmt_ctx_(NULL)
{
    stream_parsers.reserve(8); //reserve for 8 streams
}
FFmpegDemuxer::~FFmpegDemuxer()
{
    
}
int FFmpegDemuxer::Open(const std::string &input, 
             const std::string &ffmpeg_options_str,
             int io_timeout)
{
    int ret;
    AVDictionary *format_opts = NULL;
    
    if(!io_enabled()){
        return FFMPEG_SOURCE_ERR_IO;
    }
    
    //parse ffmpeg_options_str
    ret = av_dict_parse_string(&format_opts, 
                               ffmpeg_options_str.c_str(),
                               "=", ",", 0);
    if(ret){

        //log
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Failed to parse the ffmpeg_options_str:%s to av_dict(ret:%d)\n", ret);    
        ret = FFMPEG_SOURCE_ERR_GENERAL;                   
        goto err_out1;
    }
    
    //allocate the format context
    fmt_ctx_ = avformat_alloc_context();
    if(fmt_ctx_ == NULL){
        //log        
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR, 
                   "Failed to allocate AVFormatContext structure\n");           
        ret = FFMPEG_SOURCE_ERR_GENERAL;

        goto err_out1;        
    }
    //install the io interrupt callback function
    fmt_ctx_->interrupt_callback.callback = FFmpegDemuxer::StaticIOInterruptCB;
    fmt_ctx_->interrupt_callback.opaque = this;
    io_timeout_ = io_timeout;
    
    // open input file
    StartIO();
    ret = avformat_open_input(&fmt_ctx_, input.c_str(), NULL, &format_opts);
    StopIO();
    if (ret < 0) {
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
            "FFmpegDemuxer::Open(): Could not open source file(ret:%d) %s\n", 
            ret, input.c_str());
        ret = FFMPEG_SOURCE_ERR_IO;
        goto err_out2;   
    }
    
    if(format_opts != NULL){
        av_dict_free(&format_opts);
        format_opts = NULL;
    }    
    

    // get stream information 
    StartIO();    
    ret = avformat_find_stream_info(fmt_ctx_, NULL);
    StopIO();    
    if (ret < 0) {
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
            "FFmpegDemuxer::Open(): Could not find stream information(ret:%d)\n", 
            ret);
        ret = FFMPEG_SOURCE_ERR_IO;
        goto err_out2;  
    }    
   
    
    // parse the stream info into metadata    
    
    return 0;
    
err_out2:
    avformat_close_input(&fmt_ctx_);
    
err_out1:
    if(format_opts != NULL){
        av_dict_free(&format_opts);
        format_opts = NULL;
    }

    return ret;
}


void FFmpegDemuxer::Close()
{
    
    
    avformat_close_input(&fmt_ctx_);
    
}
int FFmpegDemuxer::ReadPacket(stream_switch::MediaFrameInfo *frame_info, 
                              AVPacket *pkt)
{
    return 0;
}

/*
void FFmpegDemuxer::FreePacket(DemuxerPacket * packet)
{
    if(packet == NULL || packet->priv == NULL){
        return;
    }
    av_free_packet((AVPacket *)packet->priv);
    free(packet->priv);
    packet->priv = NULL;
    packet->data = NULL;
    packet->size = 0;
}
*/
int FFmpegDemuxer::ReadMeta(stream_switch::StreamMetadata * meta)
{
    return 0;
}



    
void FFmpegDemuxer::set_io_enabled(bool io_enabled)
{
    io_enabled_ = io_enabled;
}
bool FFmpegDemuxer::io_enabled()
{
    return io_enabled_;
}
    

void FFmpegDemuxer::StartIO()
{
    clock_gettime(CLOCK_MONOTONIC, &io_start_ts_);
    
}
void FFmpegDemuxer::StopIO()
{
    io_start_ts_.tv_sec = 0;
    io_start_ts_.tv_nsec = 0;
}
int FFmpegDemuxer::StaticIOInterruptCB(void* user_data)
{
    FFmpegDemuxer *demuxer = (FFmpegDemuxer *)user_data;
    return demuxer->IOInterruptCB();
}

int FFmpegDemuxer::IOInterruptCB()
{
    if(!io_enabled()){
        return 1;
    }
    if(io_start_ts_.tv_sec != 0){
        struct timespec cur_ts;
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);
        if(cur_ts.tv_sec - io_start_ts_.tv_sec > io_timeout_){
            return 1;
        }
    }
    
    return 0;
    
}

