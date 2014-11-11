/**
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
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
 * stsw_stream_source.cc
 *      StreamSource class implementation file, define all methods of StreamSource.
 * 
 * author: jamken
 * date: 2014-11-8
**/ 

#include<stsw_stream_source.h>




namespace stream_switch {

struct ClientListType{
    int 1;
}    
    
    
    
    
StreamSource::StreamSource()
:api_socket(NULL), publish_socket(NULL), api_thread(NULL), flags(0), cur_bps(0), 
last_frame_sec(0), last_frame_usec(0)， stream_state(SOURCE_STREAM_STATE_CONNECTING)
{
    
    
}

StreamSource::~StreamSource()
{
    
}