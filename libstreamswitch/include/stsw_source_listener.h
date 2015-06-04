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
 * stsw_source_listener.h
 *      Define the source listener interface 
 * 
 * author: jamken
 * date: 2014-11-8
**/ 


#ifndef STSW_SOURCE_LISTENER_H
#define STSW_SOURCE_LISTENER_H

#include<stsw_defs.h>
#include<stdint.h>




namespace stream_switch {




// the Source Listener class
//     An interface to handle stream source's callback. 
// When some event happens, the stream source would 
// invokes the specific method of its listener in its 
// internal thread context. 
    
class SourceListener{
public:    
    // OnKeyFrame
    // When the source receive a key frame request from receivers, 
    // this medthod would be invoked
    virtual void OnKeyFrame(void) = 0;
    
    // OnMediaStatistic
    // When receiving a PROTO_PACKET_CODE_MEDIA_STATISTIC request, 
    // it would be invoked to update statistic info from application
    // The parameter statistic is initialized with the internal infomation
    // of the source instance
    virtual void OnMediaStatistic(MediaStatisticInfo *statistic) = 0;    
 
};

}

#endif