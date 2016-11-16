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
 * stsw_ffmpeg_source_global.cc
 *      The header file includes some global definitions and declarations
 * used by other parts of ffmpeg_demuxer_source
 * 
 * author: OpenSight Team
 * date: 2015-10-22
**/ 

#ifndef STSW_FFMPEG_SOURCE_GLOBAL_H
#define STSW_FFMPEG_SOURCE_GLOBAL_H


enum FFmpegSourceErrCode{    
    FFMPEG_SOURCE_ERR_OK = 0, 
    FFMPEG_SOURCE_ERR_GENERAL = -1, // general error
    FFMPEG_SOURCE_ERR_TIMEOUT = -2, // general error
    

    FFMPEG_SOURCE_ERR_EOF = -64,    // Resource EOF
    FFMPEG_SOURCE_ERR_DROP = -65,  // packet is dropped internal, need request again   
    FFMPEG_SOURCE_ERR_IO = -66,   // IO operation fail
    
    FFMPEG_SOURCE_ERR_NOT_FOUND = -67,  // Resource not found    
    FFMPEG_SOURCE_ERR_GAP = -68,  //local time gap 
};


    

#endif