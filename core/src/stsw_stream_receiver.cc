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
 * stsw_stream_receiver.cc
 *      StreamReceiver class implementation file, define all methods of StreamReceiver.
 * 
 * author: jamken
 * date: 2014-12-5
**/ 

#include <stsw_stream_receiver.h>
#include <stdint.h>
#include <list>
#include <string.h>
#include <errno.h>

#include <czmq.h>

#include <stsw_lock_guard.h>

#include <pb_packet.pb.h>
#include <pb_client_heartbeat.pb.h>
#include <pb_media.pb.h>
#include <pb_stream_info.pb.h>
#include <pb_metadata.pb.h>
#include <pb_media_statistic.pb.h>

namespace stream_switch {

StreamReceiver::StreamReceiver()
:api_socket_(NULL), client_hearbeat_socket_(NULL), subscriber_socket_(NULL),
worker_thread_id_(0), ssrc(0), flags_(0), 
last_heartbeat_time_(0), next_send_client_heartbeat_msec(0)
{
    
}


StreamReceiver::~StreamReceiver()
{
    
}




}





