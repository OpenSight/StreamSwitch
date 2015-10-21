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
 * stsw_ffmpeg_demuxer_source.cc
 *      FfmpegDemuxerSource class implementation file, includes all method 
 * implemenation of FfmpegDemuxerSource. 
 *      FfmpegDemuxerSource is a stream source implementation which acquire the 
 * media data through ffmpeg libavformat library with demuxing. 
 * only support live stream by now
 * 
 * author: jamken
 * date: 2015-10-18
**/ 
#include "stsw_ffmpeg_demuxer_source.h"

FFmpegDemuxerSource::FFmpegDemuxerSource()
demuxer_(NULL), live_thread_id_(0), io_timeout_(0), is_started_(false), 
native_frame_rate_(false), on_error_fun_(NULL), user_data_(NULL)
{
    
}



FFmpegDemuxerSource::~FFmpegDemuxerSource()
{
    if(demuxer_ != NULL){
        delete demuxer_;
        demuxer_ = NULL;
    }
}


FFmpegDemuxerSource::Init(std::string input, 
             std::string stream_name, 
             std::string ffmpeg_options_str,
             int local_gap_max_time, 
             int io_timeout,
             bool native_frame_rate, 
             int source_tcp_port, 
             int queue_size, 
             int debug_flags)
{
                 
}

void FFmpegDemuxerSource::Uninit();
int FFmpegDemuxerSource::Start(OnErrorFun on_error_fun, void *user_data);
void FFmpegDemuxerSource::Stop();
