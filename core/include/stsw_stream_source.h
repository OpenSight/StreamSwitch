#ifndef STSW_STREAM_SOURCE_H
#define STSW_STREAM_SOURCE_H
#include<czmq.h>

namespace stream_switch {
    class StreamSource{
    public:
    private:
        zsock_t * api_socket;
        zsock_t * publish_socket;
        void * mutex;
        void * api_handler_map;
        void * staitistic;
        void * stream_meta;
        uint32_t flags; /*bit 0: meta data ready*/
        int64_t last_frame_sec;
        int32_t last_frame_usec;
        int stream_state;
        void * clientList;
        
               
    }
}

#endif