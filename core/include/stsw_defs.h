#ifndef STSW_DEFS_H
#define STSW_DEFS_H

#include <stdio.h>

namespace stream_switch {
    
class StreamSource;
class ProtoCommonPacket;

typedef int (*SourceApiHandler)(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);

struct SourceApiHandlerEntry{
    SourceApiHandler handler;
    void * user_data;
};


enum SourceStreamState {
  SOURCE_STREAM_STATE_CONNECTING = 0,
  SOURCE_STREAM_STATE_OK = 1,
  SOURCE_STREAM_STATE_ERR = -1,
  SOURCE_STREAM_STATE_ERR_CONNECT_FAIL = -2,
  SOURCE_STREAM_STATE_ERR_MEIDA_STOP = -3,
  SOURCE_STREAM_STATE_ERR_TIME = -4
};


enum StreamPlayType{
    STREAM_PLAY_TYPE_LIVE = 0,
    STREAM_PLAY_TYPE_REPLAY = 1    
};


enum SourceSubStreamMediaType{
    SUB_STREAM_MEIDA_TYPE_VIDEO = 0;
    SUB_STREAM_MEIDA_TYPE_AUDIO = 1;
    SUB_STREAM_MEIDA_TYPE_TEXT = 2;
    SUB_STREAM_MEIDA_TYPE_PRIVATE = 3;
}



}

#endif