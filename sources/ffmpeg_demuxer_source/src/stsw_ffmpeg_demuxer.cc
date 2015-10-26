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

FFmpegDemuxer::FFmpegDemuxer()
{
    
}
FFmpegDemuxer::~FFmpegDemuxer()
{
    
}
int FFmpegDemuxer::Open(const std::string &input, 
             const std::string &ffmpeg_options_str,
             int io_timeout)
{
    return 0;
}


void FFmpegDemuxer::Close()
{
    
}
int FFmpegDemuxer::ReadPacket(DemuxerPacket * packet)
{
    return 0;
}
void FFmpegDemuxer::FreePacket(DemuxerPacket * packet)
{
    
}
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
    


