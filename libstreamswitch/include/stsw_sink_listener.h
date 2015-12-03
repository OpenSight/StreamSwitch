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
 * stsw_receiver_listener.h
 *      Define the receiver listener interface 
 * 
 * author: OpenSight Team
 * date: 2014-11-8
**/ 


#ifndef STSW_RECEIVER_LISTENER_H
#define STSW_RECEIVER_LISTENER_H

#include<stsw_defs.h>
#include<stdint.h>




namespace stream_switch {




// the Receiver Listener class
//     An interface to handle stream receiver's callback. 
// When some event happens, the stream receiver would 
// invokes the specific method of its listener in its 
// internal thread context. 
// Note: User cannot invoke Init/Uninit/Start/Stop method of the associated stream
// sink in callbacks. Otherwie, a deadlock would occurs. 
class SinkListener{
public:    
    // OnLiveMediaFrame
    // When a live media frame is received, this method would be invoked
    // by the internal thread
    // Args: 
    //     media_frame MediaFrameInfo in : info of the media frame received
    //     frame_data : the frame data of the media, whose format depends on 
    //                  which codec in used. It should be the encoded raw data 
    //                  of one intact frame
    //                  Notes:
    //                  1) For H264/h265, the frame data consist of one or more
    //                     (start code (00 00 00 01) + NAL) unit, but at most one 
    //                     VCL which includes a intact picture. 
    //                  2) For MPEG4, it's a segment of MPEG4 bitstream 
    //                     including just one intact picture

    virtual void OnLiveMediaFrame(const MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size) = 0;    
    

    
    // OnMetadataMismatch()
    // When the received live media frame does not match with
    // the sink's metadata, this method would be invoked
    // by the internal thread
    // Params:
    //    mismatch_ssrc - The ssrc in the received frame which is mismatch
    virtual void OnMetadataMismatch(uint32_t mismatch_ssrc) = 0;                                      
 
};

}

#endif