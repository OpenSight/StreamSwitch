#ifndef STSW_DEFS_H
#define STSW_DEFS_H

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>
#include <sys/time.h>

#define STSW_SOCKET_NAME_STREAM_PREFIX  "stsw_stream"
#define STSW_SOCKET_NAME_CONNECTOR  "/"
#define STSW_SOCKET_NAME_STREAM_API  "api"
#define STSW_SOCKET_NAME_STREAM_PUBLISH  "broadcast"
#define STSW_PUBLISH_MEDIA_CHANNEL "media"
#define STSW_PUBLISH_INFO_CHANNEL "info"


#define STSW_PUBLISH_SOCKET_HWM  250
#define STSW_SUBSCRIBE_SOCKET_HWM  250
#define STSW_PUBLISH_SOCKET_LINGER  100    

#define STSW_CLIENT_LEASE  15  //client heartbeat lease, in sec

#define STSW_MAX_CLIENT_NUM  32767  //the max client num for one source

namespace stream_switch {
    
class StreamSource;
class StreamSink;
class ProtoCommonPacket;
class ArgParser;

typedef void * SocketHandle;
typedef void * ThreadHandle;
typedef void * LockHandle;



typedef int (*SourceApiHandler)(void * user_data, const ProtoCommonPacket * request, ProtoCommonPacket * reply);

struct SourceApiHandlerEntry{
    SourceApiHandler handler;
    void * user_data;
};


typedef int (*SinkSubHandler)( void * user_data, const ProtoCommonPacket * msg);

struct SinkSubHandlerEntry{
    SinkSubHandler handler;
    std::string channel_name;
    void * user_data;
};


typedef int (*OptionHandler)(ArgParser *parser, const std::string &opt_name, 
                            const char * opt_value, void * user_data);


struct ArgParserOptionsEntry{
    std::string opt_name;
    int opt_key;
#define OPTION_FLAG_WITH_ARG      1    /* this option require a arg*/
#define OPTION_FLAG_OPTIONAL_ARG  2    /* this option can has a optional arg*/ 
#define OPTION_FLAG_REQUIRED      4    /* this option is required */ 
#define OPTION_FLAG_LONG          8    /* this option value is long type */
#define OPTION_FLAG_PREFER        0x10    /* this option value is prefer. 
 *                                         if some options is prefer, 
 *                                         don't check REQUIRED option */
    uint32_t flags;
    std::string value_name;
    std::string help_info; 
    OptionHandler user_parse_handler;
    void * user_data;
};


typedef void (*SignalHandler)(int signal_num);



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
    STREAM_PLAY_TYPE_LIVE = 0,
    STREAM_PLAY_TYPE_REPLAY = 1,    
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
    std::string font_type; 
    TextMediaParam(){
        x = 0;
        y = 0;
        fone_size = 0;      
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
        play_type = STREAM_PLAY_TYPE_LIVE;
        ssrc = 0;
        bps = 0;
    }
                                             
};  
    
 struct SubStreamMediaStatistic{
    int32_t sub_stream_index;
    SubStreamMediaType media_type;
     
    //about bytes
    uint64_t data_bytes;  //the bytes received of this sub stream
    uint64_t key_bytes; //the bytes in the key frames of this sub stream. key_frame_bytes <= received_bytes
    
    //about frame
    uint64_t lost_frames;    // the count of the lost data frames
    uint64_t data_frames;    // the count of the data frames handled
    uint64_t key_frames;   // the count of the key frames handled
    uint64_t last_gov;           //last gov  
    
    uint64_t cur_gov;            //current calculating gov, internal used by StreamSource
    uint64_t last_seq;     //the last frame's seq number, internal used by StreamSource
    
    SubStreamMediaStatistic(){
        sub_stream_index = 0;
        media_type = SUB_STREAM_MEIDA_TYPE_VIDEO;
        lost_frames = 0;
        data_bytes = 0;
        key_bytes = 0;
        data_frames = 0;
        key_frames = 0;
        last_gov = 0;
        cur_gov = 0;
        last_seq = 0;
    }
    
};  
typedef std::vector<SubStreamMediaStatistic> SubStreamMediaStatisticVector;




struct MediaStatisticInfo{
    uint32_t ssrc;
    int64_t timestamp;   //the statistic generation time
    uint64_t sum_bytes;  //the sum bytes received of all sub streams    
    SubStreamMediaStatisticVector sub_streams;
};
       
    
enum MediaFrameType{
    MEDIA_FRAME_TYPE_KEY_FRAME = 0,      //key data frame of this stream
    MEDIA_FRAME_TYPE_DATA_FRAME = 1,     //normal data frame of the stream
    MEDIA_FRAME_TYPE_PARAM_FRAME = 2,    //frame only include some codec parameter of the stream
    
    MEDIA_FRAME_TYPE_EOF_FRAME = 256,  //A special frame type means reach the end of the media stream, 
                                         //no valid media data in this message
};    


struct MediaFrameInfo{
    
    // the sub stream index in the configured metadata
    int32_t sub_stream_index;    
    
     
    //frame type of this frame
    MediaFrameType frame_type;
    
    //pts for this frame. if the stream is live stream, this timestamp is 
    // the absolute timestamp from epoch. If replay stream, this timestamp
    // is the relative time from the stream's beginning. 
    struct timeval timestamp; 
    
    // must match the ssrc in the metadata of source
    uint32_t ssrc; 
    
    MediaFrameInfo()
    :sub_stream_index(0), frame_type(MEDIA_FRAME_TYPE_DATA_FRAME),
     ssrc(0)
    {
        timestamp.tv_sec = 0;
        timestamp.tv_usec = 0;
    } 
};


enum StreamClientIPVersion {
    STREAM_IP_VERSION_V4 = 0,
    STREAM_IP_VERSION_V6 = 1,    
};

struct StreamClientInfo{
    StreamClientIPVersion client_ip_version;         //client IP version
    std::string client_ip;       // client ip address, ipv4/ipv6
    int32_t client_port;      //client source port
    std::string client_token;     // client token, used to identify client with same IP and port
    std::string client_protocol; //stream media protocol used by the client
    std::string client_text;  // text to describe this connected client
    int64_t last_active_time; //last active timestamp of this client    
    
    StreamClientInfo(){
        client_ip_version = STREAM_IP_VERSION_V4;
        client_ip = "127.0.0.1";
        client_port = 0;
        client_protocol = "uninit";
        client_text = "";
        last_active_time = 0;
    }
    
};

typedef std::list<StreamClientInfo> StreamClientList;


#define DEBUG_FLAG_DUMP_API          1     //dump the api request/reply, except client hearbeat
#define DEBUG_FLAG_DUMP_PUBLISH      2     //dump api request/reply, and publish/subsribe message
#define DEBUG_FLAG_DUMP_HEARTBEAT    4     //dump client heartbeat 


enum LogLevel{
    LOG_LEVEL_EMERG = 0,    
    LOG_LEVEL_ALERT = 1,        
    LOG_LEVEL_CRIT = 2,    
    LOG_LEVEL_ERR = 3,       
    LOG_LEVEL_WARNING = 4, 
    LOG_LEVEL_NOTICE = 5,    
    LOG_LEVEL_INFO = 6,       
    LOG_LEVEL_DEBUG = 7,        
};
#define MAX_LOG_ROW_SIZE 1024

enum ErrorCode{
    ERROR_CODE_OK = 0,    //successful
    ERROR_CODE_GENERAL = -1,   //general error
    ERROR_CODE_TIMEOUT = -2,   //timeout error
    ERROR_CODE_PARAM  = -3,    //parameter check error
    ERROR_CODE_CLIENT = -4,    //remote server reported client error
    ERROR_CODE_SERVER = -5,    //remote server reported internal error
    ERROR_CODE_SYSTEM = -6,    //the low level system API report error    
    ERROR_CODE_BUSY = -7,      //system is busy now, retry later  
    ERROR_CODE_PARSE = -8,     //protocol parse error
    ERROR_CODE_OPTIONS = -9,   //option parse error
    
};

#define SET_ERR_INFO(err_info, str_value) if(err_info) (*err_info) = str_value;
#define SAFE_DELETE(obj) if(obj != NULL) {delete obj; obj = NULL;}


}

#endif