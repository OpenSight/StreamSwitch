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
 * stsw_output_sink.h
 *      MediaOutputSink class header file, declare all the interface of 
 *  MediaOutputSink 
 * 
 * author: jamken
 * date: 2015-4-3
**/ 


#ifndef STSW_OUTPUT_SINK_H
#define STSW_OUTPUT_SINK_H

#ifndef STREAM_SWITCH
#define STREAM_SWITCH
#endif
#include "liveMedia.hh"

////////// PtsSessionNormalizer and PtsSubsessionNormalizer definitions //////////

// the Media Output Sink class
//     this class is used to get the media frame from the underlayer source
// perform some intermediate processing, and output them to the parent rtsp 
// client.
// It's reponsieble for 
// 1) check the PTS is monotonic, otherwise drop the frame
// 2) analyze the packet, and update the metadata of parent rtsp client. 

class MediaOutputSink: public MediaSink {

};


#endif
