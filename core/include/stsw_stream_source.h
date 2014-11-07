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

#ifndef STSW_STREAM_SOURCE_H
#define STSW_STREAM_SOURCE_H
#include<czmq.h>
#include<pthreahd.h>

namespace stream_switch {
    class StreamSource{
    public:
        void init(void);
        void uninit(void);
        void set_stream_meta(void);
        void get_stream_meta(void);
        void start_api_thread(void);
        void stop_api_thread(void);
        void send_media_data(void);
        void send_stream_info(void);
        void set_stream_state(void);
        
        
        void key_frame(void);
        void get_packet_statistic(void);
        
    protected:
        zsock_t * api_socket;
        zsock_t * publish_socket;
        pthread_mutex_t lock;
        pthread_t * api_thread;        
        void * api_handler_map;
        uint32_t flags; /*bit 0: meta data ready*/        
        void * staitistic;
        
        void * stream_meta;
        uint32_t cur_bps;
        int64_t last_frame_sec;
        int32_t last_frame_usec;
        int stream_state;
        void * clientList;

        
               
    }
}

#endif