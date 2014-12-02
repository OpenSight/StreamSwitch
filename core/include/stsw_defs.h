#ifndef STSW_DEFS_H
#define STSW_DEFS_H

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>

#define STSW_SOCKET_NAME_STREAM_PREFIX  "stsw_stream"
#define STSW_SOCKET_NAME_CONNECTOR  "/"
#define STSW_SOCKET_NAME_STREAM_API  "api"
#define STSW_SOCKET_NAME_STREAM_PUBLISH  "broadcast"
#define STSW_PUBLISH_MEDIA_CHANNEL "media"
#define STSW_PUBLISH_INFO_CHANNEL "info"


#define STSW_PUBLISH_SOCKET_HWM  250
#define STSW_PUBLISH_SOCKET_LINGER  100    

#define STSW_CLIENT_LEASE  15  //client heartbeat lease, in sec

namespace stream_switch {
    
class StreamSource;
class ProtoCommonPacket;


typedef void * SocketHandle;
typedef void * ThreadHandle;
typedef void * LockHandle;


typedef int (*SourceApiHandler)(StreamSource *source, const ProtoCommonPacket * request, ProtoCommonPacket * reply, void * user_data);

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
    
    VideoMediaParam(){
        height = 0;
        width = 0;
        fps = 0;
        gov = 0;
    }
};


struct AudioMediaParam{
    uint32_t samples_per_second;  //sample frequency, in Hz
    uint32_t channels;   //channel number
    uint32_t bits_per_sample; // bits per sample    
    uint32_t sampele_per_frame;  // sample per frame
    AudioMediaParam(){
        samples_per_second = 0;
        channels = 0;
        bits_per_sample = 0;
        sampele_per_frame = 0;
    }
};


struct TextMediaParam{
    uint32_t x;  //text top-left position, in video picture
    uint32_t y;   //text top-left position, in video picture
    uint32_t fone_size; 
    uint32_t font_type; 
    TextMediaParam(){
        x = 0;
        y = 0;
        fone_size = 0;
        font_type = 0;        
    }
};




struct SubStreamMetadata{

    int32_t sub_stream_index;
    SubStreamMediaType media_type;
    std::string codec_name;
    SubStreamDirectionType direction;
    std::string extra_data;
    
    struct {
        VideoMediaParam video;
        AudioMediaParam audio;
        TextMediaParam text;
    } media_param;
    
    SubStreamMetadata(){
        sub_stream_index = 0;
        media_type = SUB_STREAM_MEIDA_TYPE_VIDEO;
        direction = SUB_STREAM_DIRECTION_OUTBOUND;
    }
       
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
    uint64_t expected_frames;  // the total frames should be received
    uint64_t total_frames;    // the received frame number
    uint64_t key_frames;   // the received key frame number
    uint64_t last_gov;           //last gov  
    uint64_t cur_gov;            //current calculating gov  
    
    uint64_t last_seq;     //the last frame's seq number
    
    SubStreamMediaStatistic(){
        total_bytes = 0;
        key_bytes = 0;
        total_frames = 0;
        key_frames = 0;
        last_gov = 0;
        cur_gov = 0;
        expected_frames = 0;
        last_seq = 0;
    }
    
};  
typedef std::vector<SubStreamMediaStatistic> SubStreamMediaStatisticVector;
    
       
    
enum MediaFrameType{
    MEDIA_FRAME_TYPE_KEY_FRAME = 0,      //The key frame of this stream
    MEDIA_FRAME_TYPE_DATA_FRAME = 1,     //normal data frame of the stream
    MEDIA_FRAME_TYPE_PARAM_FRAME = 2,    //frame only include some codec parameter of the stream
   
};    



enum ErrorCode{
    ERROR_CODE_OK = 0,    //successful
    ERROR_CODE_GENERAL = -1,   //general error
    ERROR_CODE_TIMEOUT = -2,   //timeout error
    ERROR_CODE_PARAM  = -3,    //parameter check error
    ERROR_CODE_SERVER = -4,    //remote server reported error
    
};

#define SET_ERR_INFO(err_info, str_value) if(err_info) (*err_info) = str_value;

}

#endif