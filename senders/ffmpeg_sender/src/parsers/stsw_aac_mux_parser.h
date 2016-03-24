/**
 * This file is part of ffmpeg_sender, which belongs to StreamSwitch
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
 * stsw_aac_mux_parser.h
 *      AacMuxParser class header file, define intefaces of the AacMuxParser 
 * class. 
 *      AacMuxParser is sub class of StreamMuxParser, which is reponsible to 
 * handle the aac codec
 * 
 * author: OpenSight Team
 * date: 2016-2-3
**/ 

#ifndef STSW_AAC_MUX_PARSER_H
#define STSW_AAC_MUX_PARSER_H


#include <time.h>
#include <stream_switch.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
#ifdef __cplusplus
}
#endif
#include "stsw_stream_mux_parser.h"


///////////////////////////////////////////////////////////////
//Type


class AacMuxParser:public StreamMuxParser{
  
public:
    AacMuxParser();
    virtual ~AacMuxParser();
    
    virtual int Init(FFmpegMuxer * muxer, 
                     const std::string &codec_name, 
                     const stream_switch::SubStreamMetadata &sub_metadata, 
                     AVFormatContext *fmt_ctx);
    virtual void Uninit();
    virtual void Flush();    
    
    virtual int Parse(const stream_switch::MediaFrameInfo *frame_info, 
                      const char * frame_data,
                      size_t frame_size,
                      struct timeval *base_timestamp,
                      AVPacket *opkt);


protected:
    virtual int InitInputContext(const stream_switch::SubStreamMetadata &sub_metadata);
    virtual int InitOutputContext(FFmpegMuxer * muxer, 
                                  const std::string &codec_name, 
                                  AVFormatContext *fmt_ctx);  
    virtual int InitResampler();
    virtual int InitAudioFifo();
    
    virtual int InitInputPacket(const stream_switch::MediaFrameInfo *frame_info, 
                                const char * frame_data,
                                size_t frame_size,
                                struct timeval *base_timestamp,
                                AVPacket *input_pkt);   

    virtual int EncodeAudioFrame(AVFrame *frame, int *data_present);  
    
    virtual int FlushInternal();
    
    virtual int ReadAudioFifo(void ** data, int nb_samples, int64_t *pts); 
    virtual int WriteAudioFifo(void ** data, int nb_samples, int64_t pts);
    virtual void CleanupAudioFifo();    
    virtual bool CheckAudioFifoPts(int64_t pts, int64_t sample_rate);
   
    bool transcoding_;
    AVCodecContext *in_codec_context_;
    AVCodecContext *out_codec_context_;   
    SwrContext *resample_context_; 
    AVAudioFifo *fifo_;
    int64_t fifo_pts_;
    AVFrame *input_frame_;
    AVFrame *output_frame_;    
};


#endif