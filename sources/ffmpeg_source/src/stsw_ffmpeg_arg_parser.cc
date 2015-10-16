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
 * stsw_ffmpeg_arg_parser.cc
 *      FFmpegArgParser class implementation file, FFmpegArgParser is 
 * the an arg parser for ffmpeg_source application. FFmpegArgParser 
 * extends the ArgParser to parse the ffmpeg options which start by 
 * "--ffmpeg-". 
 * 
 * author: jamken
 * date: 2014-10-16
**/ 

#include "stsw_arg_parser.h"
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>



static bool is_long(const char *str)
{
    char* p;
    strtol(str, &p, 0);
    return *p == 0;    
}
    
    
FFmpegArgParser::FFmpegArgParser()
{

}



FFmpegArgParser::~FFmpegArgParser()
{
    
}



void FFmpegArgParser::RegisterSourceOptions()
{
    ArgParser::RegisterSourceOptions();
    
    //register the other options

                   
    
}
    





bool FFmpegArgParser::ParseUnknown(const char * unknown_arg)
{
    //just ignore, user can override this function to print error message and exit
    return false;
}





