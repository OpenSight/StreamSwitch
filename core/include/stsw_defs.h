#ifndef STSW_DEFS_H
#define STSW_DEFS_H

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <list>

#define STSW_SOCKET_NAME_STREAM_PREFIX  "stsw_stream"
#define STSW_SOCKET_NAME_CONNECTOR  "/"
#define STSW_SOCKET_NAME_STREAM_API  "api"
#define STSW_SOCKET_NAME_STREAM_PUBLISH  "broadcast"


namespace stream_switch {
    
class StreamSource;
class ProtoCommonPacket;


typedef void * SocketHandle;
typedef void * ThreadHandle;
typedef void * LockHandle;


typedef int (*SourceApiHandler)(StreamSource * source, ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);

struct SourceApiHandlerEntry{
    SourceApiHandler handler;
    void * user_data;
};


enum SourceStreamState {
  SOURCE_STREAM_STATE_CONNECTING = 0,
  SOURCE_STREAM_STATE_OK = 1,
  
  //error
  SOURCE_STREAM_STATE_ERR = -1,
  SOURCE_STREAM_STATE_ERR_CONNECT_FAIL = -2,
  SOURCE_STREAM_STATE_ERR_MEIDA_STOP = -3,
  SOURCE_STREAM_STATE_ERR_TIME = -4
};



enum StreamPlayType{
    Stream_PLAY_TYPE_LIVE = 0,
    Stream_PLAY_TYPE_REPLAY = 1,    
};

enum SubStreamMediaType{
    SUB_STREAM_MEIDA_TYPE_VIDEO = 0,
    SUB_STREAM_MEIDA_TYPE_AUDIO = 1,
    SUB_STREAM_MEIDA_TYPE_TEXT = 2,
    SUB_STREAM_MEIDA_TYPE_PRIVATE = 3,
};


enum SubStreamDirectionType{
    SUB_STREAM_DIRECTION_OUTBOUND = 0,
    SUB_STREAM_DIRECTION_INBOUND = 1,  // to support send audio from receiver to the  source
};


struct VideoMediaParam{
    uint32_t height;  //frame height
    uint32_t width;   //frame width
    uint32_t fps;     
    uint32_t gov;  
};


struct AudioMediaParam{
    uint32_t samples_per_second;  //sample frequency, in Hz
    uint32_t channels;   //channel number
    uint32_t bits_per_sample; // bits per sample    
    uint32_t sampele_per_frame;  // sample per frame
};


struct TextMediaParam{
    uint32_t x;  //text top-left position, in video picture
    uint32_t y;   //text top-left position, in video picture
    uint32_t fone_size; 
    uint32_t font_type;  
};




struct SubStreamMetadata{

    int32_t sub_stream_index;
    SubStreamMediaType media_type;
    std::string codec_name;
    SubStreamDirectionType direction;
    std::string extra_data;
    
    union{
        VideoMediaParam video;
        AudioMediaParam audio;
        TextMediaParam text;
    } media_param;
       
};  

typedef std::vector<SubStreamMetadata> SubStreamMetadataVector;

    
struct StreamMetadata{
    StreamPlayType play_type;   //playing type of this stream
    std::string source_proto;   //the protocol of this source
    uint32_t ssrc;              //ssrc of this stream, if the receive frame must
                                // has the same ssrc with this stream, otherwise
                                // the media frame cannot be described by this 
                                //meta data
    uint32_t bps; //announced bps for the total throughput of this stream, 0 
                  // means the source has no announce for throughput  

    SubStreamMetadataVector sub_streams; 
    
    StreamMetadata(){
        play_type = Stream_PLAY_TYPE_LIVE;
        ssrc = 0;
        bps = 0;
    }
                                             
};  
    
 struct SubStreamMediaStatistic{
    //about bytes
    uint64_t total_bytes;  //the bytes received of this sub stream
    uint64_t key_bytes; //the bytes in the key frames of this sub stream. key_frame_bytes <= received_bytes
    
    //about frame
    uint64_t total_frames;    // the received frame number
    uint64_t key_frames;   // the received key frame number
    uint64_t last_gov;           //last gov  
    uint64_t cur_gov;            //current calculating gov  
    
    
    
};  
typedef std::vector<SubStreamMediaStatistic> SubStreamMediaStatisticVector;
    
       
    
    

}

#endif