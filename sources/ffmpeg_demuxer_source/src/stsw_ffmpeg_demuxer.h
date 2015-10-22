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
 *      FfmpegDemuxer class header file, define intefaces of the FfmpegDemuxer 
 * class. 
 *      FFmpegDemuxer is a media file demuxer based on ffmpeg libavformat 
 * 
 * author: jamken
 * date: 2015-10-15
**/ 

#ifndef STSW_FFMPEG_DEMUX_H
#define STSW_FFMPEG_DEMUX_H


#include <stdint.h>
#include <string>
#include <stream_switch.h>


struct DemuxerPacket{
    stream_switch::MediaFrameInfo frame_info;
    uint8_t * 	data; 
    int 	size;
    void * priv;
    DemuxerPacket(){
        data = NULL;
        size = 0;
        priv = NULL;
    }
};

class FFmpegDemuxer{
public:
    FFmpegDemuxer();
    virtual ~FFmpegDemuxer();
    int Open(const std::string &input, 
             const std::string &ffmpeg_options_str,
             int io_timeout);
    int Close();
    int ReadPacket(DemuxerPacket * packet);
    void FreePacket(DemuxerPacket * packet);
    int ReadMeta(stream_switch::StreamMetadata * meta);

};
    

