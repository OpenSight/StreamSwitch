/**
 * This file is part of stsw_rtsp_port, which belongs to StreamSwitch
 * project. And it's derived from Feng prject originally
 * 
 * Copyright (C) 2015  OpenSight team (www.opensight.cn)
 * Copyright (C) 2009 by LScube team <team@lscube.org>
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 **/
#include <string>
#include <math.h>

#include "fnc_log.h"
#include "media/demuxer.h"
#include "media/mediaparser.h"
#include "media/mediaparser_module.h"
#include "put_bits.h"
#include "network/rtsp.h"

#include "feng.h"

#include "stream_switch.h"

#define MP2P_MAX_STREAM_NUM 4
#define MP2P_MAX_PES_SIZE 5120



#define AUDIO_ID 0xc0
#define VIDEO_ID 0xe0

#define PACK_START_CODE             ((unsigned int)0x000001ba)
#define SYSTEM_HEADER_START_CODE    ((unsigned int)0x000001bb)
#define SYSTEM_MAP_HEADER_START_CODE    ((unsigned int)0x000001bc)
#define SEQUENCE_END_CODE           ((unsigned int)0x000001b7)
#define PACKET_START_CODE_MASK      ((unsigned int)0xffffff00)
#define PACKET_START_CODE_PREFIX    ((unsigned int)0x00000100)
#define ISO_11172_END_CODE          ((unsigned int)0x000001b9)



class StreamParser{
public:

    static StreamParser * CreateParser(const std::string &codec_name, uint8_t stream_id, uint8_t stream_type, 
                 int media_type, int max_buffer_size);
    

    virtual int Parse(Track *tr, uint8_t *data, size_t len);
    virtual void Reset(Track *tr){
        //nothing to do by default
        return;
    }
    virtual bool IsKeyFrame(uint8_t *data, size_t len){
        // no key frame by default
        return false;
    }
    
    virtual uint8_t stream_id(){
        return stream_id_;
    }
    
    virtual uint8_t stream_type(){
        return stream_type_;
    }
    virtual int max_buffer_size(){
        return max_buffer_size_;
    }
    
protected:
    StreamParser(const std::string &codec_name, uint8_t stream_id, uint8_t stream_type, 
                 int media_type, int max_buffer_size/* in bytes */);
    virtual int EncodePes(Track *track, uint8_t *data, size_t len, 
                        bool is_key_frame,
                       uint8_t *buf, size_t bufSize, size_t maxSize);
    
    std::string codec_name_;
    uint8_t stream_id_;
    uint8_t stream_type_;
    int media_type_;
    int max_buffer_size_;
};




static const MediaParserInfo info = {
    "MP2P",
    MP_video
};



typedef struct {
    int packet_size; /* required packet size */
    int packet_number;
    int mux_rate; /* bitrate in units of 50 bytes/s */
    /* stream info */
    int audio_bound;
    int video_bound;
    int stream_num;
    StreamParser * streams[MP2P_MAX_STREAM_NUM];
    int gov_start; // if the parser has seen GOV start flag, gov_start would be set to 1
} mp2p_priv;


class DemuxerSinkListener;

typedef struct stsw_priv_type{
    stream_switch::StreamSink * sink;
    DemuxerSinkListener *listener;

    int stream_type;
  
    //for live stream time scaler
    /** Real-time timestamp when to start the playback */
    double playback_time;
    int has_sync;
    double delta_time;
    
    
} stsw_priv_type;



static void mp2p_rtp_frag(Track *tr, uint8_t *data, size_t len);




/*****************************************************************/
/* crc32 utils, copy from ffmpeg */

typedef uint32_t AVCRC;
typedef enum {
    AV_CRC_8_ATM,
    AV_CRC_16_ANSI,
    AV_CRC_16_CCITT,
    AV_CRC_32_IEEE,
    AV_CRC_32_IEEE_LE,  /*< reversed bitorder version of AV_CRC_32_IEEE */
    AV_CRC_16_ANSI_LE,  /*< reversed bitorder version of AV_CRC_16_ANSI */
    AV_CRC_24_IEEE = 12,
    AV_CRC_MAX,         /*< Not part of public API! Do not use outside libavutil. */
}AVCRCId;

static const AVCRC av_crc_table[257] = {
        0x00000000, 0xB71DC104, 0x6E3B8209, 0xD926430D, 0xDC760413, 0x6B6BC517,
        0xB24D861A, 0x0550471E, 0xB8ED0826, 0x0FF0C922, 0xD6D68A2F, 0x61CB4B2B,
        0x649B0C35, 0xD386CD31, 0x0AA08E3C, 0xBDBD4F38, 0x70DB114C, 0xC7C6D048,
        0x1EE09345, 0xA9FD5241, 0xACAD155F, 0x1BB0D45B, 0xC2969756, 0x758B5652,
        0xC836196A, 0x7F2BD86E, 0xA60D9B63, 0x11105A67, 0x14401D79, 0xA35DDC7D,
        0x7A7B9F70, 0xCD665E74, 0xE0B62398, 0x57ABE29C, 0x8E8DA191, 0x39906095,
        0x3CC0278B, 0x8BDDE68F, 0x52FBA582, 0xE5E66486, 0x585B2BBE, 0xEF46EABA,
        0x3660A9B7, 0x817D68B3, 0x842D2FAD, 0x3330EEA9, 0xEA16ADA4, 0x5D0B6CA0,
        0x906D32D4, 0x2770F3D0, 0xFE56B0DD, 0x494B71D9, 0x4C1B36C7, 0xFB06F7C3,
        0x2220B4CE, 0x953D75CA, 0x28803AF2, 0x9F9DFBF6, 0x46BBB8FB, 0xF1A679FF,
        0xF4F63EE1, 0x43EBFFE5, 0x9ACDBCE8, 0x2DD07DEC, 0x77708634, 0xC06D4730,
        0x194B043D, 0xAE56C539, 0xAB068227, 0x1C1B4323, 0xC53D002E, 0x7220C12A,
        0xCF9D8E12, 0x78804F16, 0xA1A60C1B, 0x16BBCD1F, 0x13EB8A01, 0xA4F64B05,
        0x7DD00808, 0xCACDC90C, 0x07AB9778, 0xB0B6567C, 0x69901571, 0xDE8DD475,
        0xDBDD936B, 0x6CC0526F, 0xB5E61162, 0x02FBD066, 0xBF469F5E, 0x085B5E5A,
        0xD17D1D57, 0x6660DC53, 0x63309B4D, 0xD42D5A49, 0x0D0B1944, 0xBA16D840,
        0x97C6A5AC, 0x20DB64A8, 0xF9FD27A5, 0x4EE0E6A1, 0x4BB0A1BF, 0xFCAD60BB,
        0x258B23B6, 0x9296E2B2, 0x2F2BAD8A, 0x98366C8E, 0x41102F83, 0xF60DEE87,
        0xF35DA999, 0x4440689D, 0x9D662B90, 0x2A7BEA94, 0xE71DB4E0, 0x500075E4,
        0x892636E9, 0x3E3BF7ED, 0x3B6BB0F3, 0x8C7671F7, 0x555032FA, 0xE24DF3FE,
        0x5FF0BCC6, 0xE8ED7DC2, 0x31CB3ECF, 0x86D6FFCB, 0x8386B8D5, 0x349B79D1,
        0xEDBD3ADC, 0x5AA0FBD8, 0xEEE00C69, 0x59FDCD6D, 0x80DB8E60, 0x37C64F64,
        0x3296087A, 0x858BC97E, 0x5CAD8A73, 0xEBB04B77, 0x560D044F, 0xE110C54B,
        0x38368646, 0x8F2B4742, 0x8A7B005C, 0x3D66C158, 0xE4408255, 0x535D4351,
        0x9E3B1D25, 0x2926DC21, 0xF0009F2C, 0x471D5E28, 0x424D1936, 0xF550D832,
        0x2C769B3F, 0x9B6B5A3B, 0x26D61503, 0x91CBD407, 0x48ED970A, 0xFFF0560E,
        0xFAA01110, 0x4DBDD014, 0x949B9319, 0x2386521D, 0x0E562FF1, 0xB94BEEF5,
        0x606DADF8, 0xD7706CFC, 0xD2202BE2, 0x653DEAE6, 0xBC1BA9EB, 0x0B0668EF,
        0xB6BB27D7, 0x01A6E6D3, 0xD880A5DE, 0x6F9D64DA, 0x6ACD23C4, 0xDDD0E2C0,
        0x04F6A1CD, 0xB3EB60C9, 0x7E8D3EBD, 0xC990FFB9, 0x10B6BCB4, 0xA7AB7DB0,
        0xA2FB3AAE, 0x15E6FBAA, 0xCCC0B8A7, 0x7BDD79A3, 0xC660369B, 0x717DF79F,
        0xA85BB492, 0x1F467596, 0x1A163288, 0xAD0BF38C, 0x742DB081, 0xC3307185,
        0x99908A5D, 0x2E8D4B59, 0xF7AB0854, 0x40B6C950, 0x45E68E4E, 0xF2FB4F4A,
        0x2BDD0C47, 0x9CC0CD43, 0x217D827B, 0x9660437F, 0x4F460072, 0xF85BC176,
        0xFD0B8668, 0x4A16476C, 0x93300461, 0x242DC565, 0xE94B9B11, 0x5E565A15,
        0x87701918, 0x306DD81C, 0x353D9F02, 0x82205E06, 0x5B061D0B, 0xEC1BDC0F,
        0x51A69337, 0xE6BB5233, 0x3F9D113E, 0x8880D03A, 0x8DD09724, 0x3ACD5620,
        0xE3EB152D, 0x54F6D429, 0x7926A9C5, 0xCE3B68C1, 0x171D2BCC, 0xA000EAC8,
        0xA550ADD6, 0x124D6CD2, 0xCB6B2FDF, 0x7C76EEDB, 0xC1CBA1E3, 0x76D660E7,
        0xAFF023EA, 0x18EDE2EE, 0x1DBDA5F0, 0xAAA064F4, 0x738627F9, 0xC49BE6FD,
        0x09FDB889, 0xBEE0798D, 0x67C63A80, 0xD0DBFB84, 0xD58BBC9A, 0x62967D9E,
        0xBBB03E93, 0x0CADFF97, 0xB110B0AF, 0x060D71AB, 0xDF2B32A6, 0x6836F3A2,
        0x6D66B4BC, 0xDA7B75B8, 0x035D36B5, 0xB440F7B1, 0x00000001
    };

const AVCRC *av_crc_get_table(AVCRCId crc_id)
{

    return av_crc_table;
}


uint32_t av_crc(const AVCRC *ctx, uint32_t crc,
                const uint8_t *buffer, size_t length)
{
    const uint8_t *end = buffer + length;

    while (buffer < end)
        crc = ctx[((uint8_t) crc) ^ *buffer++] ^ (crc >> 8);

    return crc;
}

/*****************************************************************/
/* PS utils */

/* header function */
static int put_pack_header(mp2p_priv *priv,
                           uint8_t *buf, int bufSize, int64_t timestamp)
{
    PutBitContext pb;

    init_put_bits(&pb, buf, bufSize);

    put_bits32(&pb, PACK_START_CODE);

    put_bits(&pb, 2, 0x1);

    put_bits(&pb, 3,  (uint32_t)((timestamp >> 30) & 0x07));
    put_bits(&pb, 1, 1);
    put_bits(&pb, 15, (uint32_t)((timestamp >> 15) & 0x7fff));
    put_bits(&pb, 1, 1);
    put_bits(&pb, 15, (uint32_t)((timestamp      ) & 0x7fff));
    put_bits(&pb, 1, 1);
    put_bits(&pb, 9, 0);
    put_bits(&pb, 1, 1);
    put_bits(&pb, 22, priv->mux_rate);
    put_bits(&pb, 1, 1);

    put_bits(&pb, 1, 1);
    put_bits(&pb, 5, 0x1f); /* reserved */
    put_bits(&pb, 3, 0); /* stuffing length */

    flush_put_bits(&pb);

    return put_bits_ptr(&pb) - pb.buf;
}





static int put_system_header(mp2p_priv *priv, uint8_t *buf,int bufSize)
{
    int size, i, private_stream_coded, id;
    PutBitContext pb;

    init_put_bits(&pb, buf, bufSize);

    put_bits32(&pb, SYSTEM_HEADER_START_CODE);
    put_bits(&pb, 16, 0);
    put_bits(&pb, 1, 1);

    put_bits(&pb, 22, priv->mux_rate); /* maximum bit rate of the multiplexed stream */
    put_bits(&pb, 1, 1); /* marker */

    put_bits(&pb, 6, priv->audio_bound);


    put_bits(&pb, 1, 0); /* variable bitrate*/
    put_bits(&pb, 1, 0); /* constrainted bit stream */

    put_bits(&pb, 1, 0); /* audio locked */
    put_bits(&pb, 1, 0); /* video locked */

    put_bits(&pb, 1, 1); /* marker */

    put_bits(&pb, 5, priv->video_bound);

    put_bits(&pb, 8, 0xff); /* reserved byte */


        /* audio stream info */
    private_stream_coded = 0;
    for(i=0;i<priv->stream_num;i++) {
        StreamParser *stream_parser = priv->streams[i];

        if(stream_parser != NULL) {
            
            id = stream_parser->stream_id();
            if (id < 0xc0) {
                /* special case for private streams (AC-3 uses that) */
                if (private_stream_coded)
                    continue;
                private_stream_coded = 1;
                id = 0xbd;
            }
            put_bits(&pb, 8, id); /* stream ID */
            put_bits(&pb, 2, 3);
            if (id < 0xe0) {
                /* audio */
                put_bits(&pb, 1, 0);
                put_bits(&pb, 13, stream_parser->max_buffer_size() / 128);
            } else {
                /* video */
                put_bits(&pb, 1, 1);
                put_bits(&pb, 13, stream_parser->max_buffer_size() / 1024);
            }
        }
        
    }

    flush_put_bits(&pb);
    size = put_bits_ptr(&pb) - pb.buf;
    /* patch packet size */
    buf[4] = (size - 6) >> 8;
    buf[5] = (size - 6) & 0xff;

    return size;
}



static int put_system_map_header(mp2p_priv *priv, uint8_t *buf,int bufSize)
{
    int size, nonCRCsize, i, streamNum;
    PutBitContext pb;
    const AVCRC *ctx;
    uint32_t crc;

    init_put_bits(&pb, buf, bufSize);

    put_bits32(&pb, SYSTEM_MAP_HEADER_START_CODE);

    put_bits(&pb, 16, 0);

    put_bits(&pb, 1, 1); //current next indicator

    put_bits(&pb, 2, 3); //reserve

    put_bits(&pb, 5, 0); //program stream map version 

    put_bits(&pb, 7, 0x7f); //reserve 

    put_bits(&pb, 1, 1); //marker

	put_bits(&pb, 16, 0);    //programe stream info length

    streamNum = 0;
    for(i=0;i<priv->stream_num;i++) {
        StreamParser *stream_parser = priv->streams[i];

        if(stream_parser != NULL) {
            streamNum++;
        }
    }

	put_bits(&pb, 16, streamNum * 4);    //elementary stream map length 

    for(i=0;i<priv->stream_num;i++) {
        StreamParser *stream_parser = priv->streams[i];

        if(stream_parser != NULL) {
            put_bits(&pb, 8, stream_parser->stream_type());
            put_bits(&pb, 8, stream_parser->stream_id());
            put_bits(&pb, 16, 0);    //elementary_stream_info_length
        }
    }


    flush_put_bits(&pb);
    nonCRCsize = put_bits_ptr(&pb) - pb.buf;
    size = nonCRCsize + 4;
    /* patch packet size */
    buf[4] = (size - 6) >> 8;
    buf[5] = (size - 6) & 0xff;

    /* calculate crc32 */
    ctx = av_crc_get_table(AV_CRC_32_IEEE);
    crc = av_crc(ctx, 0, buf, nonCRCsize);
    
    

    buf[nonCRCsize] = (crc) & 0xff;
    buf[nonCRCsize+1] = (crc >> 8) & 0xff;
    buf[nonCRCsize+2] = (crc >> 16) & 0xff;
    buf[nonCRCsize+3] = (crc >> 24) & 0xff;

    /* for debug */
#if 0
    ctx = av_crc_get_table(AV_CRC_32_IEEE);
    crc = av_crc(ctx, 0, buf, size);
    printf("crc = %X\n", crc);    
#endif

    return size;
}



static int put_pes_header(uint8_t streamId, uint32_t pesSize, uint32_t stuffSize, 
                          uint8_t first_PES_frag, 
                          uint8_t data_alignment_indicator, 
                          uint8_t dwPtsDtsFlag, int64_t pts, int64_t dts,
                          int max_buffer_size,
                          uint8_t *buf,int bufSize)
{
    int i;
    uint32_t header_data_length;
    PutBitContext pb;

    init_put_bits(&pb, buf, bufSize);

    put_bits32(&pb, PACKET_START_CODE_PREFIX | streamId );

    put_bits(&pb, 16, pesSize & 0xffff);

    put_bits(&pb, 2, 2); //10

    put_bits(&pb, 2, 0); //PES_scrambling_control


    if ((streamId & 0xe0) == VIDEO_ID){
        put_bits(&pb, 1, 1); //PES_priority 
    }else{
        put_bits(&pb, 1, 0); //PES_priority 
    }


    put_bits(&pb, 1, data_alignment_indicator & 0x1 ); //data_alignment_indicator

//    put_bits(&pb, 1, 0); //PES_priority 

//    put_bits(&pb, 1, 0 ); //data_alignment_indicator

    put_bits(&pb, 1, 0); //copyright

    put_bits(&pb, 1, 0); //original_or_copy

	put_bits(&pb, 2, dwPtsDtsFlag & 0x3);    //PTS_DTS_flags

//	put_bits(&pb, 6, 0);    //other flags

    put_bits(&pb, 1, 0);
    put_bits(&pb, 1, 0);
    put_bits(&pb, 1, 0);
    put_bits(&pb, 1, 0);
    put_bits(&pb, 1, 0);
    if(first_PES_frag) {
        put_bits(&pb, 1, 1);     //PES_extension_flag
    }else{
        put_bits(&pb, 1, 0);     //PES_extension_flag
    }
    header_data_length = stuffSize;
    if(first_PES_frag) {
        header_data_length += 3;
    }

	if(dwPtsDtsFlag==2) {//10b
        header_data_length += 5;
        put_bits(&pb, 8, header_data_length & 0xff);
        put_bits(&pb, 4, 2);
        put_bits(&pb, 3,  (uint32_t)((pts >> 30) & 0x07));
        put_bits(&pb, 1, 1);
        put_bits(&pb, 15, (uint32_t)((pts >> 15) & 0x7fff));
        put_bits(&pb, 1, 1);
        put_bits(&pb, 15, (uint32_t)((pts      ) & 0x7fff));
        put_bits(&pb, 1, 1);

    }else if(dwPtsDtsFlag==3){ //11b
        header_data_length += 10;
        put_bits(&pb, 8, header_data_length & 0xff);

        put_bits(&pb, 4, 3);
        put_bits(&pb, 3,  (uint32_t)((pts >> 30) & 0x07));
        put_bits(&pb, 1, 1);
        put_bits(&pb, 15, (uint32_t)((pts >> 15) & 0x7fff));
        put_bits(&pb, 1, 1);
        put_bits(&pb, 15, (uint32_t)((pts      ) & 0x7fff));
        put_bits(&pb, 1, 1);

        put_bits(&pb, 4, 1);
        put_bits(&pb, 3,  (uint32_t)((dts >> 30) & 0x07));
        put_bits(&pb, 1, 1);
        put_bits(&pb, 15, (uint32_t)((dts >> 15) & 0x7fff));
        put_bits(&pb, 1, 1);
        put_bits(&pb, 15, (uint32_t)((dts      ) & 0x7fff));
        put_bits(&pb, 1, 1);

    }else{
        put_bits(&pb, 8, header_data_length & 0xff);

    }
    
    if(first_PES_frag) {
        put_bits(&pb, 8, 0x10);
        if ((streamId & 0xe0) == AUDIO_ID){
    
            put_bits(&pb, 16, 0x4000 | max_buffer_size/ 128);
    
        }else{
    
            put_bits(&pb, 16, 0x6000 | max_buffer_size/ 1024);
        }
    }


    for(i=0;i < stuffSize; i++ ) {
        put_bits(&pb, 8, 0xff);
    }

    flush_put_bits(&pb);

    return put_bits_ptr(&pb) - pb.buf;
}

static int get_pes_header_size(int preSize, uint8_t dwPtsDtsFlag, uint8_t first_PES_frag, uint32_t *stuffSize)
{
    int headerSize = 0;
    int align;

    headerSize += 9;

	if(dwPtsDtsFlag==2) {//10b
        headerSize += 5;
    }else if(dwPtsDtsFlag==3){ //11b
        headerSize += 10;

    }
    if(first_PES_frag) {
        headerSize += 3; /* pes extension */
    }

    align = (preSize + headerSize + 1 + 3) & ~(0x3);
    *stuffSize = align - headerSize - preSize;
    headerSize += *stuffSize;

    return headerSize;

}


static void put_word(uint32_t word, uint8_t *buf)
{
    int i = 0;
    buf[i++] = (word >> 24) & 0xff;
    buf[i++] = (word >> 16) & 0xff;
    buf[i++] = (word >> 8) & 0xff;
    buf[i++] = (word) & 0xff;
}

static inline uint64_t pts2PSTimpstamp(Track *tr, 
                                       double pts)
{
    unsigned int clock_rate;
    clock_rate = tr->properties.clock_rate;
    double playback_time;
    
    //jamken: use the pts of the frame directly to generate 
    //PES timestamp, don't scale
#if 0
    if(tr != NULL && 
	   tr->parent != NULL &&
	   tr->parent->rtsp_sess != NULL &&
	   ((RTSP_session *) tr->parent->rtsp_sess)->scale != 1.0) {
        pts = pts / (double)(((RTSP_session *) tr->parent->rtsp_sess)->scale);

    }
#endif
    if(tr->properties.media_source == MS_live){
        
        //for live  stream, convert pts to the wall time at first
        
        stsw_priv_type * demuxer_priv = (stsw_priv_type *)tr->parent->private_data;
        
        return (uint64_t)((pts + demuxer_priv->playback_time) * clock_rate);
        
    }else{
        return (uint64_t)(pts * clock_rate);

    }
    return 0;
}

/****************************************************************/
/*stream parser */

StreamParser * StreamParser::CreateParser(const std::string &codec_name, 
                                           uint8_t stream_id, uint8_t stream_type, 
                                           int media_type, int max_buffer_size)
{
    return new StreamParser(codec_name, stream_id, stream_type, media_type, max_buffer_size);
}

StreamParser::StreamParser(const std::string &codec_name, 
                           uint8_t stream_id, uint8_t stream_type, 
                           int media_type, int max_buffer_size/* in bytes */)
:codec_name_(codec_name), stream_id_(stream_id), stream_type_(stream_type),
media_type_(media_type), max_buffer_size_(max_buffer_size)
{
    
}


int StreamParser::Parse(Track *tr, uint8_t *data, size_t len)
{
    using namespace stream_switch;
    mp2p_priv * priv = NULL;
    int ret;
    uint8_t *buf;
    size_t maxBufSize,bufSize;
    int64_t timestamp;
    int retSize;
    bool is_key_frame;

    priv = (mp2p_priv *)tr->private_data;
   
    
    /* malloc space*/
    maxBufSize = 0;
    maxBufSize += 256; /* pack, system, map header size */
    maxBufSize += ((len + priv->packet_size -100 -1)/ (priv->packet_size -100)) * 256; /* pes headers */
    maxBufSize += len;
    maxBufSize += 1024; /* add 1 K extraly */
    buf = (uint8_t *)g_malloc(maxBufSize);
    if(buf == NULL) {
        fnc_log(FNC_LOG_ERR, "[mp2p] StreamParser.parse() malloc failed");
        ret = RESOURCE_DAMAGED;
        goto error1;
    }
    memset(buf, 0, maxBufSize);
    bufSize = 0;

    timestamp = pts2PSTimpstamp(tr, tr->properties.pts);

    
    /* add pack header */
    retSize = put_pack_header(priv, buf+bufSize, maxBufSize - bufSize, timestamp);
    bufSize += retSize;

    /* check if key frame, then add system header and map header */
    is_key_frame = IsKeyFrame(data, len);
    if(is_key_frame && media_type_ == SUB_STREAM_MEIDA_TYPE_VIDEO) {
        
        retSize = put_system_header(priv, buf+bufSize, maxBufSize - bufSize);
        bufSize += retSize;        

        retSize = put_system_map_header(priv,buf+bufSize, maxBufSize - bufSize);
        bufSize += retSize;
        
        //key frame start a new gov
        priv->gov_start = 1;
    }

    retSize = EncodePes(tr, data, len, is_key_frame, buf, bufSize, maxBufSize );
    if(retSize < 0 ) {
        fnc_log(FNC_LOG_VERBOSE, "[mp2p] Encode Pes error");
        ret = RESOURCE_NOT_PARSEABLE;
        goto error2;
    }
    bufSize += retSize;


    /* frags */
    if(retSize > 0 && priv->gov_start != 0) { /* means some data is added */
        mp2p_rtp_frag(tr,buf,bufSize);
        fnc_log(FNC_LOG_VERBOSE, "[mp2p] Frame completed");
    }

    g_free(buf);


    return ERR_NOERROR;

error2:
    
    g_free(buf);
error1:
    return ret;    
}

int StreamParser::EncodePes(Track *track, uint8_t *data, size_t len, 
                       bool is_key_frame, 
                       uint8_t *buf, size_t bufSize, size_t maxSize)
{
    using namespace stream_switch;
    int64_t pts, dts;
    mp2p_priv * priv = (mp2p_priv * )track->private_data;
    size_t orgBufSize = bufSize;
    int onlyKeyFrame = 0;
    uint8_t dwPtsDtsFlag;


    if(track->parent != NULL && track->parent->rtsp_sess != NULL) {
        onlyKeyFrame = ((RTSP_session *)track->parent->rtsp_sess)->onlyKeyFrame;
    }

    pts = pts2PSTimpstamp(track, track->properties.pts);
    dts = pts2PSTimpstamp(track, track->properties.dts);

    dwPtsDtsFlag = 2;
    

    if(onlyKeyFrame == 0 ||
       is_key_frame ||
       media_type_ == SUB_STREAM_MEIDA_TYPE_AUDIO) {
        /* frame: (data), size: len */

        uint8_t first_PES_frag = 1;
        size_t index = 0;

        while(index < len) {

            int pes_header_size = 0;
            uint32_t stuffSize = 0;
            size_t partial_size = 0;
            size_t pes_size = 0;
            uint8_t data_alignment_indicator = 0;
            

            pes_size = 0;
            partial_size = 0;
            pes_header_size = 0;
            stuffSize = 0;


            /* calulate PES size */
            pes_header_size = get_pes_header_size(bufSize,dwPtsDtsFlag, first_PES_frag, &stuffSize);
            pes_size += pes_header_size;

            if(pes_size + (len - index) > priv->packet_size) {
                partial_size = priv->packet_size - pes_size;
            }else{
                partial_size = len - index;

            }
            pes_size += partial_size;


            /* check overflow */
            if(pes_size + bufSize > maxSize) {
                /* error, not enough space */
                fnc_log(FNC_LOG_ERR, "[mp2p] StreamParser::EncodePes(): not enough space to hold pes (%d/%d)", 
                        (int)pes_size + bufSize, (int)maxSize);
                return -1;
            }
            
            
            if(media_type_ == SUB_STREAM_MEIDA_TYPE_VIDEO){
                data_alignment_indicator = first_PES_frag;
            }else{
                data_alignment_indicator = 0;
            }
            /* fill in PES header */
            pes_header_size = put_pes_header(stream_id(), 
                                             pes_size - 6, stuffSize, 
                                             first_PES_frag,
                                             data_alignment_indicator, 
                                             dwPtsDtsFlag, pts, dts, 
                                             max_buffer_size(),
                                             buf + bufSize, maxSize - bufSize);
            bufSize += pes_header_size;
                

            /* fill in frame */
            memcpy(buf + bufSize, (data + index), partial_size);
            bufSize += partial_size;

            /* clean up */
            first_PES_frag = 0; /* the first pes of the NALU has data_alignment_indicator*/
            dwPtsDtsFlag = 0; /* only the first pes has pts */
            index += partial_size;
                
        }

    }

    return (int)(bufSize - orgBufSize);
                           
}


// mp4v parser
class Mp4vStreamParser: public StreamParser{

public:
    static StreamParser * CreateParser(const std::string &codec_name, uint8_t stream_id, 
                                       uint8_t stream_type, int media_type, int max_buffer_size);
    
    virtual bool IsKeyFrame(uint8_t *data, size_t len);
protected:
    Mp4vStreamParser(const std::string &codec_name, 
                     uint8_t stream_id, uint8_t stream_type, 
                     int media_type, int max_buffer_size);
};

extern "C" int isMp4vKeyFrame(uint8_t * frameSource, size_t frameSize);

bool Mp4vStreamParser::IsKeyFrame(uint8_t *data, size_t len)
{
    return (isMp4vKeyFrame(data,len) != 0);
}

Mp4vStreamParser::Mp4vStreamParser(const std::string &codec_name, 
                     uint8_t stream_id, uint8_t stream_type, 
                     int media_type, int max_buffer_size)
:StreamParser(codec_name, stream_id, stream_type, media_type, max_buffer_size)
{
    
}

StreamParser * Mp4vStreamParser::CreateParser(const std::string &codec_name, uint8_t stream_id, 
                                         uint8_t stream_type, int media_type, int max_buffer_size)
{
    return new Mp4vStreamParser(codec_name, stream_id, stream_type, 
                                media_type, max_buffer_size);
}


#define MAX_CACHE_SIZE  (5*1024*1024)
// h264 parser
class H264StreamParser: public StreamParser{

public:

    static StreamParser * CreateParser(const std::string &codec_name, uint8_t stream_id, 
                                       uint8_t stream_type, int media_type, int max_buffer_size);
    
    virtual bool IsKeyFrame(uint8_t *data, size_t len)
    {
        return is_key_frame_;
    }
    
    virtual int Parse(Track *tr, uint8_t *data, size_t len);
    virtual void Reset(Track *tr){
        frame_cache_.clear();
        cache_dts_ = 0;
        cache_pts_ = 0;
        is_key_frame_ = false;
        return;
    }    
    virtual int FlushCache(Track *tr);
    virtual int AppendCache(Track *tr, uint8_t *data, size_t len);
    
    virtual bool IsVCL(uint8_t *data, size_t len){
        uint8_t nal_unit_type;
        if(data == NULL || len == 0){
            return false;
        }
        nal_unit_type =  data[0] & 0x1F;
        if(nal_unit_type <= 5){
            return true;
        }else{
            return false;
        }
    }
    virtual bool IsIDR(uint8_t *data, size_t len){
        uint8_t nal_unit_type;
        if(data == NULL || len == 0){
            return false;
        }
        nal_unit_type =  data[0] & 0x1F;
        if(nal_unit_type == 5){
            return true;
        }else{
            return false;
        }        
    }
    
protected:
    H264StreamParser(const std::string &codec_name, 
                     uint8_t stream_id, uint8_t stream_type, 
                     int media_type, int max_buffer_size);
                     
protected:
    bool is_key_frame_;
    std::string frame_cache_;
    double cache_pts_;             //time is in seconds
    double cache_dts_;             //time is in seconds
    
};


H264StreamParser::H264StreamParser(const std::string &codec_name, 
                     uint8_t stream_id, uint8_t stream_type, 
                     int media_type, int max_buffer_size)
:StreamParser(codec_name, stream_id, stream_type, media_type, max_buffer_size), 
is_key_frame_(false), cache_pts_(0.0), cache_dts_(0.0)
{
    
}

int H264StreamParser::Parse(Track *tr, uint8_t *data, size_t len)
{
    double pts = tr->properties.pts;             
    double dts = tr->properties.dts; 
    int ret = ERR_NOERROR;
    
           
    //
    // 1 if cannot merge with the current frame, flush the cache
    //
    if(!frame_cache_.empty() && 
       (fabs(pts - cache_pts_) >= 0.002 || fabs(dts-cache_dts_) >= 0.002)){
        FlushCache(tr);
    }
    
    //
    // 2 add the current frame to the cache
    //
    if(AppendCache(tr, data, len)){
        return ERR_ALLOC;    
    }
    is_key_frame_ = IsIDR(data, len);
    
    //
    // 3 flush the cache for VCL
    if(IsVCL(data, len)){
        ret = FlushCache(tr);
    }    
    
    return ret;
}

int H264StreamParser::AppendCache(Track *tr, uint8_t *data, size_t len)
{
    char start_code[] = {0, 0, 0, 1};
    bool is_cache_empty = frame_cache_.empty();
    
    if(frame_cache_.size() >= MAX_CACHE_SIZE){
        return -1;
    }
    
    frame_cache_.append(start_code, 4);
    frame_cache_.append((const char *)data, len);
    if(is_cache_empty){
        cache_pts_ = tr->properties.pts;
        cache_dts_ = tr->properties.dts;
    }
    return 0;
}

int H264StreamParser::FlushCache(Track *tr)
{
    double org_pts = tr->properties.pts;
    double org_dts = tr->properties.dts;
    int ret = ERR_NOERROR;
    
    if(frame_cache_.empty()){
        return ERR_NOERROR;
    }
    
    tr->properties.pts = cache_pts_;
    tr->properties.dts = cache_dts_;
    
    ret = StreamParser::Parse(tr, (uint8_t *)frame_cache_.data(), frame_cache_.size());
    
    tr->properties.pts = org_pts;
    tr->properties.dts = org_dts;
    frame_cache_.clear();
    
    return ret;
    
}





StreamParser * H264StreamParser::CreateParser(const std::string &codec_name, uint8_t stream_id, 
                                         uint8_t stream_type, int media_type, int max_buffer_size)
{
    return new H264StreamParser(codec_name, stream_id, stream_type, 
                                media_type, max_buffer_size);
}





// parser repo

struct StreamParserInfo{
    const char *codec_name;
    uint8_t stream_type;
    
    StreamParser * (*factory)(const std::string &codec_name, 
                              uint8_t stream_id, uint8_t stream_type, 
                              int media_type, int max_buffer_size);
} ;

static const StreamParserInfo mp2p_stream_parsers[] = {
    { "H264", 0x1b, H264StreamParser::CreateParser},
    { "MP4V-ES", 0x10, Mp4vStreamParser::CreateParser},
    { "PCMU", 0x90, StreamParser::CreateParser},
    { "PCMA", 0x90, StreamParser::CreateParser},
    { NULL, 0, NULL}//XXX ...
};


static StreamParser * CreateStreamParser(const std::string &codec_name, uint8_t stream_id, 
                                         int media_type, int max_buffer_size)
{
    
    const StreamParserInfo * parser_info = mp2p_stream_parsers;
    while(parser_info->codec_name != NULL){
        if(codec_name == parser_info->codec_name && 
           parser_info->factory != NULL){
            return parser_info->factory(codec_name, stream_id, parser_info->stream_type, 
                                        media_type, max_buffer_size);
        }
        parser_info ++;
    }
    return NULL;
}




/**********************************************************************************/
/* mp2p parser */


static int mp2p_init(Track *track)
{
    using namespace stream_switch;  
    const char * encoder;
    mp2p_priv * priv = NULL;
    int bitrate, i, mpa_id, mpv_id;
    int ret;
    int streamType = MP2P_STREAM;
    stream_switch::StreamMetadata meta;
    
    if(track == NULL || track->parent == NULL ||
    track->parent->demuxer == NULL || track->parent->demuxer->info == NULL ||
    track->parent->demuxer->info->short_name == NULL){
        fnc_log(FNC_LOG_ERR, "[mp2p] Cannot support the demuxer without short name\n");
        ret = ERR_INPUT_PARAM;
        goto error1;          
    }
    
    if(strcmp(track->parent->demuxer->info->short_name, "stsw") != 0){
        fnc_log(FNC_LOG_ERR, "[mp2p] Cannot support the demuxer (%s)\n", 
         track->parent->demuxer->info->short_name);
        ret = ERR_INPUT_PARAM;
        goto error1;        
    }
    
    if(track->parent->private_data != NULL ) {
        stsw_priv_type * demuxer_priv = (stsw_priv_type *)track->parent->private_data;
        meta = demuxer_priv->sink->stream_meta();
        streamType = demuxer_priv->stream_type;
    }else{
        fnc_log(FNC_LOG_ERR, "[mp2p] parameter error \n");
        ret = ERR_INPUT_PARAM;
        goto error1;
    }
    
    if(meta.sub_streams.size() > MP2P_MAX_STREAM_NUM){
        fnc_log(FNC_LOG_ERR, "[mp2p] stream number is too much, not supported\n");
        ret = ERR_PARSE;
        goto error1;
    }

    priv = g_slice_new0(mp2p_priv);
    if(priv == NULL) {
        fnc_log(FNC_LOG_ERR, "[mp2p] g_slice_new failed !!!\n");
        ret =  ERR_ALLOC;
        goto error1;
    }

    memset(priv, 0, sizeof(mp2p_priv));
    priv->packet_size = MP2P_MAX_PES_SIZE;
    priv->stream_num = meta.sub_streams.size();


    bitrate = 0;
    priv->audio_bound = 0;
    priv->video_bound = 0;
    mpa_id = AUDIO_ID;
    mpv_id = VIDEO_ID;

    for(i=0;i<priv->stream_num;i++) {
        int codec_rate;
        stream_switch::SubStreamMetadata *sub_meta = &(meta.sub_streams[i]);
        int max_buffer_size = 0;
        uint8_t stream_id = 0;
        
        
        switch(sub_meta->media_type){
        case SUB_STREAM_MEIDA_TYPE_AUDIO:
            max_buffer_size = 8 * 1024;
            stream_id = mpa_id;
            break;
        case SUB_STREAM_MEIDA_TYPE_VIDEO:
            max_buffer_size = 4 * 1024 * 1024;
            stream_id = mpv_id;
            break;
        case SUB_STREAM_MEIDA_TYPE_TEXT:
        case SUB_STREAM_MEIDA_TYPE_PRIVATE:
        default:
            stream_id = 0;
            fnc_log(FNC_LOG_WARN, "[mp2p] codec type unsupported, skip");
            continue;
            break;
        }        
        
        priv->streams[i] = CreateStreamParser(sub_meta->codec_name, stream_id, 
                                              sub_meta->media_type, max_buffer_size);
                    
        if(priv->streams[i] == NULL){
            fnc_log(FNC_LOG_WARN, "[mp2p] codec name %s is unsupported, skip", sub_meta->codec_name.c_str());
            continue;            
        }
        
        //create parser successfully
        
        switch(sub_meta->media_type){
        case SUB_STREAM_MEIDA_TYPE_AUDIO:
            mpa_id++;
            priv->audio_bound++;
            break;
        case SUB_STREAM_MEIDA_TYPE_VIDEO:
            mpv_id++;;
            priv->video_bound++;
            break;

        default:
            break;
        }

        /* bit rate*/
        codec_rate= (1<<21)*8*50/priv->stream_num;

        bitrate += codec_rate;
    }

    if(priv->video_bound == 0 ) {
        fnc_log(FNC_LOG_ERR, "[mp2p] No supported video stream is found, Fail!!");
        ret = ERR_UNSUPPORTED_PT;
        goto error2;
    }

    
    /* we increase slightly the bitrate to take into account the
           headers. XXX: compute it exactly */
    if(meta.bps != 0){
        bitrate = meta.bps ;
    }
    bitrate += bitrate / 20;
    bitrate += 10000;
    priv->mux_rate = (bitrate + (8 * 50) - 1) / (8 * 50);
    

    /* add rtp encoder map */
    encoder = "MP2P";
    if(streamType == PS_STREAM) {
        encoder = "PS";
    }else if(streamType == MP2P_STREAM){
        encoder = "MP2P";
    }
    track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("%s/%d", encoder, track->properties.clock_rate));

    track->private_data = priv;

    fnc_log(FNC_LOG_VERBOSE, "[mp2p] track init successfully");

    return ERR_NOERROR;
    
error2:
    if(priv != NULL) {
        for(int i=0;i<priv->stream_num;i++) {
            if(priv->streams[i] != NULL){
                delete priv->streams[i];
                priv->streams[i] = NULL;                
            }
        }  
       
        g_slice_free(mp2p_priv, priv);
    }

error1:
    return ret;
}

static void mp2p_rtp_frag(Track *tr, uint8_t *data, size_t len)
{
    int32_t offset, rem = len;
    int i;

    if (DEFAULT_MTU >= len) {
        mparser_buffer_write(tr,
                             tr->properties.pts,
                             tr->properties.dts,
                             tr->properties.frame_duration,
                             1,
                             data, len);
        fnc_log(FNC_LOG_VERBOSE, "[mp2p] no frags");
    } else {
        i = 0;
        do {
            offset = len - rem;
            mparser_buffer_write(tr,
                                 tr->properties.pts,
                                 tr->properties.dts,
                                 tr->properties.frame_duration,
                                 (rem <= DEFAULT_MTU),
                                 data + offset, MIN(rem, DEFAULT_MTU));
            rem -= DEFAULT_MTU;
            fnc_log(FNC_LOG_VERBOSE, "[mp2p] frags %d", i);
            i++;
        } while (rem >= 0);
    }

    return;
} 



static int mp2p_parse(Track *tr, uint8_t *data, size_t len)
{
    int index;
    mp2p_priv * priv = NULL;
    int ret;
    uint8_t *buf;
    size_t maxBufSize,bufSize;
    int64_t timestamp;
    int retSize;

    priv = (mp2p_priv *)tr->private_data;
    index = tr->info->id;
    
    if(priv->streams[index] == NULL){
        fnc_log(FNC_LOG_VERBOSE, "[mp2p] Frame skip (no stream)");
        return ERR_NOERROR;        
    }


    return priv->streams[index]->Parse(tr, data, len);


}

static void mp2p_uninit(Track *tr)
{
    if(tr->private_data) {
        mp2p_priv * priv = (mp2p_priv * )tr->private_data;
        for(int i=0;i<priv->stream_num;i++) {
            if(priv->streams[i] != NULL){
                delete priv->streams[i];
                priv->streams[i] = NULL;                
            }
        }   
 
        g_slice_free(mp2p_priv, tr->private_data);
        tr->private_data = NULL;
    }
}

static void mp2p_reset(Track *tr)
{
    if(tr->private_data) {
        mp2p_priv * priv = (mp2p_priv * )tr->private_data;
        for(int i=0;i<priv->stream_num;i++) {
            if(priv->streams[i] != NULL){
                priv->streams[i]->Reset(tr);                
            }
        }          
        priv->gov_start = 0;
    }
}

FNC_LIB_MEDIAPARSER(mp2p);

