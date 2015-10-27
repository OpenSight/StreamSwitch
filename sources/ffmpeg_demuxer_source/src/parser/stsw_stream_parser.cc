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
 * stsw_stream_parser.cc
 *      StreamParser class implementation file, define methods of the StreamParser 
 * class. 
 *      StreamParser is the default parser for a ffmpeg AV stream, other 
 * parser can inherit this class to override its methods for a specified 
 * codec. All other streams would be associated this default parser
 * 
 * author: jamken
 * date: 2015-10-27
**/ 

#include "stsw_stream_parser.h"





StreamParser::StreamParser()
{
    
}
StreamParser::~StreamParser()
{
    
}
int StreamParser::Init()
{
    return 0;
}
void StreamParser::Uninit()
{
    return 0;
}
int StreamParser::Parse()
{
    return 0;
}
int StreamParser::reset()
{
    return 0;
}








template<class T>
StreamParser* StreamParserFatcory()
{
	return new T;
}

