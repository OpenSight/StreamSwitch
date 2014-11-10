#ifndef STSW_INTERNALS_H
#define STSW_INTERNALS_H

#include <stdint.h>
#include <string>
#include <list>

namespace stream_switch{
    

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

typedef std::list<SubStreamMetadata> SubStreamMetadataList;

    
struct StreamMetadata{
    StreamPlayType play_type;   //playing type of this stream
    std::string source_proto;   //the protocol of this source
    uint32_t ssrc;              //ssrc of this stream, if the receive frame must
                                // has the same ssrc with this stream, otherwise
                                // the media frame cannot be described by this 
                                //meta data
    uint32_t bps; //announced bps for the total throughput of this stream, 0 
                  // means the source has no announce for throughput  

    SubStreamMetadataList sub_streams; 
    
    StreamMetadata(){
        play_type = Stream_PLAY_TYPE_LIVE;
        ssrc = 0;
        bps = 0;
    }
                                             
};  
    
    
    
    
    
}


#endif