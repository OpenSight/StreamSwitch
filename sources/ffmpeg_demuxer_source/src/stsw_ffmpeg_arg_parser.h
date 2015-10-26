/**
 *
 * This file is part of libstreamswtich, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *  *
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
 * stsw_ffmpeg_arg_parser.h
 *      FFmpegArgParser class header file, declare all interfaces of 
 * FFmpegArgParser. FFmpegArgParser is the an arg parser for ffmpeg_source
 * application
 * 
 * author: jamken
 * date: 2015-10-16
**/ 


#ifndef STSW_FFMPEG_ARG_PARSER_H
#define STSW_FFMPEG_ARG_PARSER_H

#include<stream_switch.h>

#include<stdint.h>
#include<map>
    

// FFmpegArgParser   
// the ffmpeg_source comdline args parser class which
// extends the ArgParser to support "--ffmpeg-" options 
// and other self arguments
class FFmpegArgParser: public stream_switch::ArgParser{
public:
    FFmpegArgParser();
    virtual ~FFmpegArgParser();

    virtual void RegisterSourceOptions();    
    const std::string & ffmpeg_options()
    {
        return ffmpeg_options_;
    }
                                

protected:               

    bool ParseUnknown(const char * unknown_arg);
       
    std::string ffmpeg_options_;  


};


#endif