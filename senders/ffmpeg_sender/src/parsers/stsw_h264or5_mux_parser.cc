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
 * stsw_h264or5_mux_parser.cc
 *      H264or5MuxParser class implementation file, define methods of the H264or5MuxParser 
 * class. 
 *      H264or5MuxParser is sub class of StreamMuxParser, which is reponsible to 
 * handle the h264 or h265 codec
 * 
 * author: jamken
 * date: 2015-12-1
**/ 

#include "stsw_h264or5_mux_parser.h"

#include <stdint.h>
#include <strings.h>

#include "dec_sps.h"
#include "../stsw_log.h"

extern "C"{

 
#include <libavcodec/avcodec.h>      
}


H264or5MuxParser::H264or5MuxParser()
:StreamMuxParser(), h_number_(264)
{

}

int H264or5MuxParser::DoExtraDataInit(FFmpegMuxer * muxer, 
                          const stream_switch::SubStreamMetadata &sub_metadata, 
                          AVFormatContext *fmt_ctx, 
                          AVStream * stream)
{   
    using namespace stream_switch; 
    AVCodecContext *c = stream->codec;
    int ret = 0;


    if(strcasecmp(sub_metadata.codec_name.c_str(), "H264") == 0){
         // get width/height from sps
        if(sub_metadata.extra_data.size() != 0){
            unsigned int width = 0;
            unsigned int height = 0;
            ret = decsps((unsigned char *)sub_metadata.extra_data.data(), 
                         sub_metadata.extra_data.size(), 
                         &width, &height);
            if(ret==0 && width !=0 && height !=0){
                //successful decode sps
                c->width    = width;
                c->height   = height; 
            }
        }       
        h_number_ = 264;
    }else if(strcasecmp(sub_metadata.codec_name.c_str(), "H265") == 0){
         // get width/height from sps
        if(sub_metadata.extra_data.size() != 0){
            unsigned int width = 0;
            unsigned int height = 0;
            ret = decsps_265((unsigned char *)sub_metadata.extra_data.data(), 
                         sub_metadata.extra_data.size(), 
                         &width, &height);
            if(ret==0 && width !=0 && height !=0){
                //successful decode sps
                c->width    = width;
                c->height   = height; 
            }
        }           
        h_number_ = 265;
    }else{
        STDERR_LOG(LOG_LEVEL_ERR, "Could not support codec:%s\n",
                    sub_metadata.codec_name.c_str());        
        return -1; 
    }
 
    return StreamMuxParser::DoExtraDataInit(muxer, sub_metadata, fmt_ctx, stream);
    
}
