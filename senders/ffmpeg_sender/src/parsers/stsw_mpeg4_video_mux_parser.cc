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
 * stsw_mpeg4_video_mux_parser.cc
 *      Mpeg4VideoMuxParser class implementation file, define methods of the Mpeg4VideoMuxParser 
 * class. 
 *      Mpeg4VideoMuxParser is sub class of StreamMuxParser, which is reponsible to 
 * handle mpeg4 video codec
 * 
 * author: OpenSight Team
 * date: 2015-12-1
**/ 

#include "stsw_mpeg4_video_mux_parser.h"

#include <stdint.h>
#include <strings.h>

#include "dec_sps.h"
#include "../stsw_log.h"

extern "C"{

 
#include <libavcodec/avcodec.h>      
}




int Mpeg4VideoMuxParser::DoExtraDataInit(FFmpegMuxer * muxer, 
                          const stream_switch::SubStreamMetadata &sub_metadata, 
                          AVFormatContext *fmt_ctx, 
                          AVStream * stream)
{   
    using namespace stream_switch; 
    int ret = 0;

    // get width/height from sps
    if(sub_metadata.extra_data.size() != 0){
        unsigned int width = 0;
        unsigned int height = 0;
        ret = decsps((unsigned char *)sub_metadata.extra_data.data(), 
                      sub_metadata.extra_data.size(), 
                      &width, &height);
        if(ret==0 && width !=0 && height !=0){
                //successful decode sps
            AVCodecContext *c = stream->codec;
            c->width    = width;
            c->height   = height; 
        }
    }       

    return StreamMuxParser::DoExtraDataInit(muxer, sub_metadata, fmt_ctx, stream);
    
}
