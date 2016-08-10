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
 * author: OpenSight Team
 * date: 2015-10-23
**/ 


#include "stsw_ffmpeg_demuxer.h"
#include "stsw_log.h"
#include "stsw_ffmpeg_source_global.h"
#include "parser/stsw_stream_parser.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


extern "C"{

//#include <libavutil/avutil.h>    
#include <libavformat/avformat.h>  
#include <libavutil/time.h>    
}


FFmpegDemuxer::FFmpegDemuxer()
:io_enabled_(true), fmt_ctx_(NULL), io_timeout_(0), 
ssrc_(0)
{
    stream_parsers_.reserve(8); //reserve for 8 streams
    stream_parsers_.clear();
    io_start_ts_.tv_nsec = 0;
    io_start_ts_.tv_sec = 0;
    
}
FFmpegDemuxer::~FFmpegDemuxer()
{
    
}
int FFmpegDemuxer::Open(const std::string &input, 
             const std::string &ffmpeg_options_str,
             unsigned long io_timeout, 
             int play_mode)
{
    using namespace stream_switch; 
    int ret;
    AVDictionary *format_opts = NULL;
    struct timeval start_time;
    const char * protocol_name;
    
    
    if(!io_enabled()){
        return FFMPEG_SOURCE_ERR_IO;
    }
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
        ret = FFMPEG_SOURCE_ERR_GENERAL;                   
        goto err_out1;
    }

    //av_dict_set(&format_opts, "rtsp_transport", "tcp", 0);
    //printf("option dict count:%d\n", av_dict_count(format_opts));
    
    
    //allocate the format context
    fmt_ctx_ = avformat_alloc_context();
    if(fmt_ctx_ == NULL){
        //log        
        STDERR_LOG(LOG_LEVEL_ERR, 
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
        STDERR_LOG(LOG_LEVEL_ERR,  
            "FFmpegDemuxer::Open(): Could not open input file(ret:%d) %s\n", 
            ret, input.c_str());
        ret = FFMPEG_SOURCE_ERR_IO;
        goto err_out2;   
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
#define DUMP_AVFORMAT
#ifdef DUMP_AVFORMAT
        STDERR_LOG(stream_switch::LOG_LEVEL_INFO,  
            "FFmpegDemuxer::Open(): avformat context is dumped below\n");    
    av_dump_format(fmt_ctx_, 0, input.c_str(), 0);
#endif  
   
    //calculate ssrc
    gettimeofday(&start_time, NULL);    
    srand(((unsigned)getpid()) << 20 + start_time.tv_usec);
    meta_.ssrc = ssrc_ = (uint32_t)(rand() % 0xffffffff);       

    // parse the stream info into metadata  
    if(play_mode == PLAY_MODE_AUTO){
        if(fmt_ctx_->duration == 0 || fmt_ctx_->duration == AV_NOPTS_VALUE){
            //live
            meta_.play_type = STREAM_PLAY_TYPE_LIVE;
            meta_.stream_len = 0.0;
        }else{
            //replay
            meta_.play_type = STREAM_PLAY_TYPE_REPLAY;
            meta_.stream_len = (double)fmt_ctx_->duration / AV_TIME_BASE;
            fmt_ctx_->flags |= AVFMT_FLAG_GENPTS; //for replay mode, make sure pts present
        }
        
    }else if(play_mode == PLAY_MODE_LIVE){
        meta_.play_type = STREAM_PLAY_TYPE_LIVE;
        meta_.stream_len = 0.0;        
        
    }else if(play_mode == PLAY_MODE_REPLAY){
        meta_.play_type = STREAM_PLAY_TYPE_REPLAY;
        fmt_ctx_->flags |= AVFMT_FLAG_GENPTS; //for replay mode, make sure pts present
        
        if(fmt_ctx_->duration == 0 || fmt_ctx_->duration == AV_NOPTS_VALUE){
            meta_.stream_len = 0.0;
        }else{
            meta_.stream_len = (double)fmt_ctx_->duration / AV_TIME_BASE;
        }        
    }
    protocol_name = avio_find_protocol_name(input.c_str());
    if(protocol_name == NULL && fmt_ctx_->iformat != NULL){
        protocol_name = fmt_ctx_->iformat->name;
    }
    if(protocol_name == NULL){
        protocol_name = "ffmpeg";
    }
    meta_.source_proto = protocol_name;
    meta_.bps = fmt_ctx_->bit_rate;
    
    for(int i=0; i<fmt_ctx_->nb_streams; i++) {
        SubStreamMetadata sub_metadata;
        AVStream *st= fmt_ctx_->streams[i];
        AVCodecContext *codec= st->codec;
        sub_metadata.codec_name = CodecNameFromId(codec->codec_id);
        sub_metadata.sub_stream_index = i;
        sub_metadata.direction = SUB_STREAM_DIRECTION_OUTBOUND;
        sub_metadata.extra_data.assign((const char *)codec->extradata, 
                                       (size_t)codec->extradata_size);
        switch(codec->codec_type){
        case AVMEDIA_TYPE_AUDIO:
            sub_metadata.media_type = SUB_STREAM_MEIDA_TYPE_AUDIO;
            sub_metadata.media_param.audio.channels = codec->channels;
            sub_metadata.media_param.audio.samples_per_second = 
                codec->sample_rate;
            sub_metadata.media_param.audio.bits_per_sample = 
                codec->bits_per_raw_sample;
            break;
        case AVMEDIA_TYPE_VIDEO:
            sub_metadata.media_type = SUB_STREAM_MEIDA_TYPE_VIDEO;
            /*frame_rate is volatile*/
            /*
            if(st->r_frame_rate.num != 0 && st->r_frame_rate.den !=0 ){
                sub_metadata.media_param.video.fps = (uint32_t)(av_q2d(st->r_frame_rate) + 0.5);                
            }*/
            if(codec->width != 0 && codec->height != 0){
                sub_metadata.media_param.video.height = codec->height;
                sub_metadata.media_param.video.width = codec->width;
            }
            break;
        case AVMEDIA_TYPE_DATA:
        case AVMEDIA_TYPE_UNKNOWN:
        case AVMEDIA_TYPE_SUBTITLE: //XXX import subtitle work!
        default:
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                    "FFmpegDemuxer::Open(): codec type (%d) unsupported \n", 
                    codec->codec_type);
            ret = FFMPEG_SOURCE_ERR_GENERAL;
            goto err_out2;
            break;
        }
        meta_.sub_streams.push_back(sub_metadata);        
    }    
    
    //setup the parsers
    for(int i=0; i<fmt_ctx_->nb_streams; i++) {
        AVStream *st= fmt_ctx_->streams[i];
        AVCodecContext *codec= st->codec;
        StreamParser *parser = NewStreamParser(codec->codec_id);
        ret = parser->Init(this, i);
        if(ret){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "Parser for codec (id:%d) init failed\n", 
                codec->codec_id); 
            delete parser;
            ret = FFMPEG_SOURCE_ERR_GENERAL;
            goto err_out3;
        }
        stream_parsers_.push_back(parser);        
    }    
    
    return 0;
    
err_out3:
    
    {
        StreamParserVector::iterator it;
        for(it = stream_parsers_.begin(); 
            it != stream_parsers_.end();
            it++){
            (*it)->Uninit();
            delete (*it);
            (*it) = NULL;
        }
        stream_parsers_.clear();
                
    }
    
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
    if(fmt_ctx_ == NULL){
        return;
    }
    
    //free all cached packet
    {
        PacketCachedList::iterator it;
        for(it = cached_pkts.begin(); 
            it != cached_pkts.end();
            it++){
            av_free_packet(&(it->pkt));
        }
        cached_pkts.clear();                
    } 
    
    //uninit all parser
    {
        StreamParserVector::iterator it;
        for(it = stream_parsers_.begin(); 
            it != stream_parsers_.end();
            it++){
            (*it)->Uninit();
            delete (*it);
            (*it) = NULL;
        }
        stream_parsers_.clear();                
    }    
    
    //close ffmpeg format context
    avformat_close_input(&fmt_ctx_);
    
}
int FFmpegDemuxer::ReadPacket(stream_switch::MediaFrameInfo *frame_info, 
                              AVPacket *pkt, bool* is_meta_changed)
{
    int ret = 0;
    int stream_index = 0;
    if(frame_info == NULL || pkt == NULL){
        return -1;
    }  
    if(fmt_ctx_ == NULL){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "FFmpegDemuxer not open\n");         
        return -1;
    } 
    
    if(!io_enabled()){
        return FFMPEG_SOURCE_ERR_IO;
    }
    if(pkt->data != NULL){
        //pkt has not yet free
        av_free_packet(pkt);
    }
 
    //check cache first
    if(cached_pkts.size() != 0){
        (*frame_info) = cached_pkts.front().frame_info;
        (*pkt) = cached_pkts.front().pkt;
        if(is_meta_changed != NULL){
            (*is_meta_changed) = false;
        }
        cached_pkts.pop_front();
        return 0;
    }
    
read_again:
    StartIO();
    ret = av_read_frame(fmt_ctx_, pkt);
    StopIO();
    if (ret == AVERROR(EAGAIN)) {
        av_usleep(10000);
        goto read_again;
    }else if(ret == AVERROR_EOF){       
        ret = FFMPEG_SOURCE_ERR_EOF;
        goto error_out1;
    }else if(ret < 0){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "av_read_frame failed with ret(%d)\n", ret);         
        ret = FFMPEG_SOURCE_ERR_IO;
        goto error_out1;        
    }

#define DUMP_PACKET    
#ifdef DUMP_PACKET
    {
        STDERR_LOG(stream_switch::LOG_LEVEL_DEBUG,  
                "Read the following packet from input file\n");         
        av_pkt_dump_log2(NULL, AV_LOG_DEBUG, pkt, 0, 
                         fmt_ctx_->streams[pkt->stream_index]);
                         
    }
#endif      
 
    stream_index = pkt->stream_index;
    if(stream_index >= stream_parsers_.size()){
        STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "packet index excceed the number of stream parsers\n");     
        ret = FFMPEG_SOURCE_ERR_GENERAL;
        goto error_out2; 
    }
    //printf("file:%s, line:%d\n", __FILE__, __LINE__);    
    ret = stream_parsers_[stream_index]->Parse(frame_info, 
                                               pkt, 
                                               is_meta_changed);
    if(ret){
        goto error_out2;   
    }
    frame_info->ssrc = ssrc_;
    //printf("file:%s, line:%d\n", __FILE__, __LINE__);        
    return 0;

error_out2:
    av_free_packet(pkt);


error_out1:
    return ret;
}


int FFmpegDemuxer::ReadMeta(stream_switch::StreamMetadata * meta, int timeout)
{
    int ret;
    struct timespec start_ts, cur_ts;
    
    if(meta == NULL){
        return FFMPEG_SOURCE_ERR_GENERAL;
    }
    if(fmt_ctx_ == NULL){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "FFmpegDemuxer not open\n");         
        return -1;
    } 
    if(!io_enabled()){
        return FFMPEG_SOURCE_ERR_IO;
    }    
    
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    ret = 0;
    while(!IsMetaReady()){
        PktNode pkt_node;
        int stream_index;
        clock_gettime(CLOCK_MONOTONIC, &cur_ts);
        if(cur_ts.tv_sec - start_ts.tv_sec >= timeout){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "Read metadata timeout for %s\n", 
                fmt_ctx_->filename);              
            ret = FFMPEG_SOURCE_ERR_TIMEOUT;
            break;
        }
        
        //init pkt_node
        av_init_packet(&(pkt_node.pkt));
        pkt_node.pkt.data = NULL;
        pkt_node.pkt.size = 0;
        
        //read the packets
        StartIO();
        ret = av_read_frame(fmt_ctx_, &(pkt_node.pkt));
        StopIO();
        if(ret == AVERROR_EOF){
            ret = FFMPEG_SOURCE_ERR_EOF;
            break;
        }else if(ret){
            ret = FFMPEG_SOURCE_ERR_IO;
            break;       
        }
    
#ifdef DUMP_PACKET
        {
            STDERR_LOG(stream_switch::LOG_LEVEL_DEBUG,  
                       "Read the following packet from input file\n");         
            av_pkt_dump_log2(NULL, AV_LOG_DEBUG, &(pkt_node.pkt), 0, 
                fmt_ctx_->streams[pkt_node.pkt.stream_index]);
                         
        }
#endif         
        stream_index = pkt_node.pkt.stream_index;
        if(stream_index >= stream_parsers_.size()){
            STDERR_LOG(stream_switch::LOG_LEVEL_ERR,  
                "packet index excceed the number of stream parsers\n");     
            ret = FFMPEG_SOURCE_ERR_GENERAL;
            av_free_packet(&(pkt_node.pkt));
            break;
        }
        ret = stream_parsers_[stream_index]->Parse(&(pkt_node.frame_info), 
                                                   &(pkt_node.pkt), 
                                                   NULL);
        if(ret == FFMPEG_SOURCE_ERR_DROP){
            av_free_packet(&(pkt_node.pkt));
            ret = 0;
            continue; //read next packet
        }else if(ret){
            av_free_packet(&(pkt_node.pkt));
            break;
        }
        pkt_node.frame_info.ssrc = ssrc_;
        
        cached_pkts.push_back(pkt_node); //cache the packet
        
    }//while(!IsMetaReady()){
    
    if(ret == 0){ //meta ready
        (*meta) = meta_;
    }   
    return ret;
}

bool FFmpegDemuxer::IsMetaReady()
{
    StreamParserVector::iterator it;
    for(it = stream_parsers_.begin(); 
        it != stream_parsers_.end();
        it++){
        if((*it)->IsMetaReady() == false){
            return false;
        }
    }
    return true;  
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
        uint64_t io_dur = (cur_ts.tv_sec - io_start_ts_.tv_sec) * 1000 +
              (cur_ts.tv_nsec - io_start_ts_.tv_nsec) / 1000000;       
        if(io_dur >= (uint64_t)io_timeout_){
            return 1;
        }
    }
    
    return 0;
    
}

