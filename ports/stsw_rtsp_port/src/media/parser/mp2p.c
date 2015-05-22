/* *
 * This file is part of Feng
 *
 * Copyright (C) 2009 by LScube team <team@lscube.org>
 * See AUTHORS for more details
 *
 * feng is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * feng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with feng; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * */
#ifdef TRISOS
#include "fnc_log.h"
#include "media/demuxer.h"
#include "media/mediaparser.h"
#include "media/mediaparser_module.h"
#include "put_bits.h"
#include "network/rtsp.h"

#include "feng.h"

#include <libavformat/avformat.h>
#include <libavutil/crc.h>

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

typedef struct mp2p_stream_struct {
    const int av_id;
    const char tag[11];
    uint8_t stream_type;
    int (*parse)(Track *track, uint8_t *data, size_t len, 
                 uint8_t *buf, size_t bufSize, size_t maxSize);
    int (*isKeyFrame)(Track *track, uint8_t *data, size_t len);

} mp2p_stream_struct;


static const MediaParserInfo info = {
    "MP2P",
    MP_video
};


typedef struct {
    uint8_t stream_id;
    uint8_t stream_type;
    int type;
    int max_buffer_size; /* in bytes */
    int (*parse)(Track *track, uint8_t *data, size_t len, 
                 uint8_t *buf, size_t bufSize, size_t maxSize);
    int (*isKeyFrame)(Track *track, uint8_t *data, size_t len);

    unsigned int nal_length_size; // used for h264 avc
} mp2p_stream_info;



typedef struct {
    int packet_size; /* required packet size */
    int packet_number;
    int mux_rate; /* bitrate in units of 50 bytes/s */
    /* stream info */
    int audio_bound;
    int video_bound;
    int stream_num;
    mp2p_stream_info stream_info[MP2P_MAX_STREAM_NUM];
} mp2p_priv;





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
        mp2p_stream_info *stream = &(priv->stream_info[i]);

        if(stream->stream_id != 0) {
            
            id = stream->stream_id;
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
                put_bits(&pb, 13, stream->max_buffer_size / 128);
            } else {
                /* video */
                put_bits(&pb, 1, 1);
                put_bits(&pb, 13, stream->max_buffer_size / 1024);
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
        mp2p_stream_info *stream = &(priv->stream_info[i]);

        if(stream->stream_id != 0) {
            streamNum++;
        }
    }

	put_bits(&pb, 16, streamNum * 4);    //elementary stream map length 

    for(i=0;i<priv->stream_num;i++) {
        mp2p_stream_info *stream = &(priv->stream_info[i]);

        if(stream->stream_id != 0) {
            put_bits(&pb, 8, stream->stream_type);
            put_bits(&pb, 8, stream->stream_id);
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

    if(tr != NULL && 
	   tr->parent != NULL &&
	   tr->parent->rtsp_sess != NULL &&
	   ((RTSP_session *) tr->parent->rtsp_sess)->scale != 1.0) {
        pts = pts / (double)(((RTSP_session *) tr->parent->rtsp_sess)->scale);

    }


    return (uint64_t)(pts * clock_rate);
}

/*********************************************************************************/
/* codec parsers */



static int ish264KeyFrame(Track *tr, uint8_t * frameSource, size_t frameSize)
{
    mp2p_priv * priv = (mp2p_priv * )tr->private_data;
    size_t nalsize = 0, index = 0;
    mp2p_stream_info * stream = &(priv->stream_info[tr->info->id]);

    
    while (1) {
        unsigned int i;
        uint8_t nal_unit_type;

        if(index >= frameSize) break;
        //get the nal size

        nalsize = 0;
        for(i = 0; i < stream->nal_length_size; i++)
            nalsize = (nalsize << 8) | frameSource[index++];
        if(nalsize <= 1 || nalsize > frameSize) {
            if(nalsize == 1) {
                index++;
                continue;
            } else {
                fnc_log(FNC_LOG_VERBOSE, "[h264] AVC: nal size %d", nalsize);
                break;
            }
        }

        nal_unit_type = frameSource[index] & 0x1F;
        if(nal_unit_type == 5 ||
           nal_unit_type == 7) {

            return 1;
        }

        index += nalsize;
    }
    return 0;
}
int isMp4vKeyFrame(uint8_t * frameSource, size_t frameSize);
static int isMp4vesKeyFrame(Track *tr, uint8_t * frameSource, size_t frameSize)
{
    return isMp4vKeyFrame(frameSource,frameSize);
}

static int noKeyFrame(Track *tr, uint8_t * frameSource, size_t frameSize)
{
    return 0;
}

static int mp2p_h264_parse(Track *tr, uint8_t *data, size_t len, 
                           uint8_t *buf, size_t bufSize, size_t maxSize)
{
    int64_t pts, dts;
    mp2p_priv * priv = (mp2p_priv * )tr->private_data;
    mp2p_stream_info * stream = &(priv->stream_info[tr->info->id]);
    size_t nalsize = 0, index = 0;
    size_t orgBufSize = bufSize;
    int onlyKeyFrame = 0;
    uint8_t dwPtsDtsFlag;

    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        onlyKeyFrame = ((RTSP_session *)tr->parent->rtsp_sess)->onlyKeyFrame;
    }

    pts = pts2PSTimpstamp(tr, tr->properties.pts);
    dts = pts2PSTimpstamp(tr, tr->properties.dts);

    dwPtsDtsFlag = 2;
    
    while (1) {
        unsigned int i;
        uint8_t nal_unit_type;

        if(index >= len) break;
        //get the nal size

        nalsize = 0;
        for(i = 0; i < stream->nal_length_size; i++)
            nalsize = (nalsize << 8) | data[index++];
        if(nalsize <= 1 || nalsize > len) {
            if(nalsize == 1) {
                index++;
                continue;
            } else {
                fnc_log(FNC_LOG_ERR, "[mp2p] AVC: nal size %d too large", nalsize);
                return -1;
                break;
            }
        }

        nal_unit_type = data[index] & 0x1F;
        if(onlyKeyFrame == 0 ||
           nal_unit_type == 5 ||
           nal_unit_type == 6 ||
           nal_unit_type == 7 ||
           nal_unit_type == 8 ||
           nal_unit_type == 9) {
            /* NALU: (data+index), size: nalsize */

            uint8_t first_PES_frag = 1;
            size_t nal_index = 0;

            first_PES_frag = 1;
            nal_index = 0;

            while(nal_index < nalsize) {

                int pes_header_size = 0;
                uint32_t stuffSize = 0;
                size_t nal_partial_size = 0;
                size_t pes_size = 0;

                pes_size = 0;
                nal_partial_size = 0;
                pes_header_size = 0;
                stuffSize = 0;

                /* calulate PES size */

                pes_header_size = get_pes_header_size(bufSize, dwPtsDtsFlag, first_PES_frag, &stuffSize);

                pes_size += pes_header_size;

                if(first_PES_frag) {
                    pes_size += 4;
                }

                if(pes_size + (nalsize - nal_index) > priv->packet_size) {
                    nal_partial_size = priv->packet_size - pes_size;
                }else{
                    nal_partial_size = nalsize - nal_index;

                }
                pes_size += nal_partial_size;


                /* check overflow */
                if(pes_size + bufSize > maxSize) {
                    /* error, not enough space */
                    fnc_log(FNC_LOG_ERR, "[mp2p] AVC: not enough space to hold pes (%d/%d)", 
                            (int)pes_size + bufSize, (int)maxSize);
                    return -1;
                }


                /* fill in PES header */
                pes_header_size = put_pes_header(stream->stream_id, 
                                                 pes_size - 6, stuffSize, 
                                                 first_PES_frag,
                                                 first_PES_frag, 
                                                 dwPtsDtsFlag, pts, dts, 
                                                 stream->max_buffer_size, 
                                                 buf + bufSize, maxSize - bufSize);
                bufSize += pes_header_size;
                

                /* fill in start code */
                if(first_PES_frag) {
                    put_word(0x00000001,buf + bufSize);
                    bufSize += 4;
                }

                /* fill in NALU */
                memcpy(buf + bufSize, (data + index) + nal_index, nal_partial_size);
                bufSize += nal_partial_size;

                /* clean up */
                first_PES_frag = 0; /* the first pes of the NALU has data_alignment_indicator*/
                dwPtsDtsFlag = 0; /* only the first pes has pts */
                nal_index += nal_partial_size;
                
            }
        }

        index += nalsize;
    }

    return (int)(bufSize - orgBufSize);

}


static int mp2p_mp4v_parse(Track *tr, uint8_t *data, size_t len, 
                           uint8_t *buf, size_t bufSize, size_t maxSize)
{
    int64_t pts, dts;
    mp2p_priv * priv = (mp2p_priv * )tr->private_data;
    mp2p_stream_info * stream = &(priv->stream_info[tr->info->id]);
    size_t orgBufSize = bufSize;
    int onlyKeyFrame = 0;
    uint8_t dwPtsDtsFlag;


    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        onlyKeyFrame = ((RTSP_session *)tr->parent->rtsp_sess)->onlyKeyFrame;
    }

    pts = pts2PSTimpstamp(tr, tr->properties.pts);
    dts = pts2PSTimpstamp(tr, tr->properties.dts);

    dwPtsDtsFlag = 2;
    

    if(onlyKeyFrame == 0 ||
       isMp4vKeyFrame(data, len)) {
        /* frame: (data), size: len */

        uint8_t first_PES_frag = 1;
        size_t index = 0;

        while(index < len) {

            int pes_header_size = 0;
            uint32_t stuffSize = 0;
            size_t vop_partial_size = 0;
            size_t pes_size = 0;

            pes_size = 0;
            vop_partial_size = 0;
            pes_header_size = 0;
            stuffSize = 0;


            /* calulate PES size */
            pes_header_size = get_pes_header_size(bufSize,dwPtsDtsFlag, first_PES_frag, &stuffSize);
            pes_size += pes_header_size;

            if(pes_size + (len - index) > priv->packet_size) {
                vop_partial_size = priv->packet_size - pes_size;
            }else{
                vop_partial_size = len - index;

            }
            pes_size += vop_partial_size;


            /* check overflow */
            if(pes_size + bufSize > maxSize) {
                /* error, not enough space */
                fnc_log(FNC_LOG_ERR, "[mp2p] Mpeg4v: not enough space to hold pes (%d/%d)", 
                        (int)pes_size + bufSize, (int)maxSize);
                return -1;
            }

            /* fill in PES header */
            pes_header_size = put_pes_header(stream->stream_id, 
                                             pes_size - 6, stuffSize, 
                                             first_PES_frag,
                                             first_PES_frag, 
                                             dwPtsDtsFlag, pts, dts, 
                                             stream->max_buffer_size,
                                             buf + bufSize, maxSize - bufSize);
            bufSize += pes_header_size;
                

            /* fill in NALU */
            memcpy(buf + bufSize, (data + index), vop_partial_size);
            bufSize += vop_partial_size;

            /* clean up */
            first_PES_frag = 0; /* the first pes of the NALU has data_alignment_indicator*/
            dwPtsDtsFlag = 0; /* only the first pes has pts */
            index += vop_partial_size;
                
        }

    }

    return (int)(bufSize - orgBufSize);

}


static int mp2p_simple_audio_parse(Track *tr, uint8_t *data, size_t len, 
                                   uint8_t *buf, size_t bufSize, size_t maxSize)
{
    int64_t pts, dts;
    mp2p_priv * priv = (mp2p_priv * )tr->private_data;
    mp2p_stream_info * stream = &(priv->stream_info[tr->info->id]);
    size_t orgBufSize = bufSize, index = 0;
    uint8_t dwPtsDtsFlag;
    uint8_t first_PES_frag = 1;


    pts = pts2PSTimpstamp(tr, tr->properties.pts);
    dts = pts2PSTimpstamp(tr, tr->properties.dts);

    dwPtsDtsFlag = 2;

    
    /* frame: (data), size: len */

    while(index < len) {

        int pes_header_size;
        uint32_t stuffSize;
        size_t partial_size = 0;
        size_t pes_size = 0;

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
            fnc_log(FNC_LOG_ERR, "[mp2p] audio: not enough space to hold pes (%d/%d)", 
                    (int)pes_size + bufSize, (int)maxSize);
            return -1;
        }

        /* fill in PES header */
        pes_header_size = put_pes_header(stream->stream_id, 
                                         pes_size - 6, stuffSize, 
                                         first_PES_frag, 
                                         0,
                                         dwPtsDtsFlag, pts, dts, 
                                         stream->max_buffer_size,
                                         buf + bufSize, maxSize - bufSize);
        bufSize += pes_header_size;
                

        /* fill in NALU */
        memcpy(buf + bufSize, (data + index), partial_size);
        bufSize += partial_size;

        /* clean up */
        dwPtsDtsFlag = 0; /* only the first pes has pts */
        first_PES_frag = 0;
        index += partial_size;
                
    }

    return (int)(bufSize - orgBufSize);

}





/**********************************************************************************/
/* mp2p parser */

static const mp2p_stream_struct mp2p_streams[] = {
    { CODEC_ID_H264, "H264", 0x1b, mp2p_h264_parse, ish264KeyFrame},
    { CODEC_ID_MPEG4, "MP4V-ES", 0x10, mp2p_mp4v_parse, isMp4vesKeyFrame},
    { CODEC_ID_PCM_MULAW, "PCMU", 0x90, mp2p_simple_audio_parse, noKeyFrame},
    { CODEC_ID_PCM_ALAW, "PCMA", 0x90, mp2p_simple_audio_parse, noKeyFrame},
    { CODEC_ID_NONE, "NONE", 0, NULL, NULL}//XXX ...
};

static const mp2p_stream_struct * find_stream_struct(int av_id)
{
    const mp2p_stream_struct *tags = mp2p_streams;
    while (tags->av_id != CODEC_ID_NONE) {
        if (tags->av_id == av_id){
            return tags;
        }
        tags++;
    }
    return NULL;
}


typedef struct lavf_priv{
//    AVInputFormat *avif;
    AVFormatContext *avfc;
//    ByteIOContext *pb;
//    int audio_streams;
//    int video_streams;
    int64_t last_pts; //Use it or not?
} lavf_priv_t;

static int mp2p_init(Track *track)
{
    AVFormatContext *avfc;
    const char * encoder;
    mp2p_priv * priv = NULL;
    int bitrate, i, mpa_id, mpv_id;
    int ret;
    int streamType = MP2P_STREAM;

    if(track!=NULL && track->parent!=NULL && track->parent->private_data != NULL &&
       ((lavf_priv_t *)track->parent->private_data)->avfc != NULL) {
        avfc = ((lavf_priv_t *)track->parent->private_data)->avfc;
    }else{
        fnc_log(FNC_LOG_ERR, "[mp2p] parameter error \n");
        ret = ERR_INPUT_PARAM;
        goto error1;
    }
    if(avfc->nb_streams > MP2P_MAX_STREAM_NUM) {
        fnc_log(FNC_LOG_ERR, "[mp2p] stream number is too much, not supported\n");
        ret = ERR_PARSE;
        goto error1;
    }

    priv = g_slice_new(mp2p_priv);
    if(priv == NULL) {
        fnc_log(FNC_LOG_ERR, "[mp2p] g_slice_new failed !!!\n");
        ret =  ERR_ALLOC;
        goto error1;
    }

    memset(priv, 0, sizeof(mp2p_priv));
    priv->packet_size = MP2P_MAX_PES_SIZE;
    priv->stream_num = avfc->nb_streams;


    bitrate = 0;
    priv->audio_bound = 0;
    priv->video_bound = 0;
    mpa_id = AUDIO_ID;
    mpv_id = VIDEO_ID;

    for(i=0;i<avfc->nb_streams;i++) {
        int codec_rate;
        AVStream *st = avfc->streams[i];
        AVCodecContext *codec= st->codec;
        const mp2p_stream_struct * stream_tag;

        stream_tag = find_stream_struct(codec->codec_id);
        if(stream_tag == NULL) {
            fnc_log(FNC_LOG_WARN, "[mp2p] codec id %d is unsupported, skip", codec->codec_id);
            continue;
        }
        
        priv->stream_info[i].stream_type = stream_tag->stream_type;
        priv->stream_info[i].parse = stream_tag->parse;
        priv->stream_info[i].isKeyFrame = stream_tag->isKeyFrame;

        switch(codec->codec_type){
        case AVMEDIA_TYPE_AUDIO:
            priv->stream_info[i].max_buffer_size = 4 * 1024;
            priv->stream_info[i].stream_id = mpa_id++;
            priv->stream_info[i].type = AVMEDIA_TYPE_AUDIO;
            priv->audio_bound++;
            break;
        case AVMEDIA_TYPE_VIDEO:

            if (codec->rc_buffer_size){
                priv->stream_info[i].max_buffer_size = 6*1024 + st->codec->rc_buffer_size/8;
            } else {
                priv->stream_info[i].max_buffer_size = 4 * 1024 * 1024;
            }

            priv->stream_info[i].stream_id = mpv_id++;;
            priv->stream_info[i].type = AVMEDIA_TYPE_VIDEO;
            priv->video_bound++;
            break;
        case AVMEDIA_TYPE_DATA:
        case AVMEDIA_TYPE_UNKNOWN:
        case AVMEDIA_TYPE_SUBTITLE: //XXX import subtitle work!
        default:
            priv->stream_info[i].stream_type = 0;
            priv->stream_info[i].parse = NULL;
            fnc_log(FNC_LOG_WARN, "[mp2p] codec type unsupported, skip");
            continue;
            break;
        }

        /* bit rate*/
        if(st->codec->rc_max_rate)
            codec_rate= st->codec->rc_max_rate;
        else
            codec_rate= st->codec->bit_rate;

        if(!codec_rate)
            codec_rate= (1<<21)*8*50/avfc->nb_streams;

        bitrate += codec_rate;

        /* h264 hack */
        if(codec->codec_id == CODEC_ID_H264) {
            if (track->properties.extradata_len == 0) {
                fnc_log(FNC_LOG_WARN, "[mp2p] No Extradata, unsupported\n");
                ret = ERR_UNSUPPORTED_PT;
                goto error2;
            }
            if(track->properties.extradata[0] != 1) {
                fnc_log(FNC_LOG_WARN, "[mp2p] only support avc format h264\n");
                ret = ERR_UNSUPPORTED_PT;
                goto error2;
            }
            if (track->properties.extradata_len < 7) {
                fnc_log(FNC_LOG_WARN, "[mp2p] h264 extra data length must >= 7\n");
                ret = ERR_UNSUPPORTED_PT;
                goto error2;
            }
            priv->stream_info[i].nal_length_size = (track->properties.extradata[4]&0x03)+1;
        }

    }

    if(priv->video_bound == 0 && priv->audio_bound == 0) {
        fnc_log(FNC_LOG_ERR, "[mp2p] No supported stream is found, Fail!!");
        ret = ERR_UNSUPPORTED_PT;
        goto error2;
    }

    
    /* we increase slightly the bitrate to take into account the
           headers. XXX: compute it exactly */
    bitrate += bitrate / 20;
    bitrate += 10000;
    priv->mux_rate = (bitrate + (8 * 50) - 1) / (8 * 50);


    

    /* add rtp encoder map */
    if(track->parent->srv != NULL) {
        streamType = track->parent->srv->srvconf.default_stream_type;
    }
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

    if(priv->stream_info[index].stream_id == 0 ||
       priv->stream_info[index].parse == NULL) {
        fnc_log(FNC_LOG_VERBOSE, "[mp2p] Frame skip (no stream)");
        return ERR_NOERROR;
    }


    /* malloc space*/
    maxBufSize = 0;
    maxBufSize += 256; /* pack, system, map header size */
    maxBufSize += ((len + priv->packet_size -100 -1)/ (priv->packet_size -100)) * 256; /* pes headers */
    maxBufSize += len;
    maxBufSize += 1024; /* add 1 K extraly */
    buf = (uint8_t *)g_malloc(maxBufSize);
    if(buf == NULL) {
        fnc_log(FNC_LOG_ERR, "[mp2p] mp2p_h264_parse malloc failed");
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
    if(priv->stream_info[index].isKeyFrame != NULL &&
       priv->stream_info[index].isKeyFrame(tr, data,len)) {
        
        retSize = put_system_header(priv, buf+bufSize, maxBufSize - bufSize);
        bufSize += retSize;        

        retSize = put_system_map_header(priv,buf+bufSize, maxBufSize - bufSize);
        bufSize += retSize;

    }

    retSize = priv->stream_info[index].parse(tr, data, len, buf, bufSize, maxBufSize);
    if(retSize < 0 ) {
        fnc_log(FNC_LOG_VERBOSE, "[mp2p] Frame parser error");
        ret = RESOURCE_NOT_PARSEABLE;
        goto error2;
    }
    bufSize += retSize;


    /* frags */
    if(retSize > 0) { /* means some data is added */
        mp2p_rtp_frag(tr,buf,bufSize);
    }

    g_free(buf);

    fnc_log(FNC_LOG_VERBOSE, "[mp2p] Frame completed");
    return ERR_NOERROR;

error2:
    
    g_free(buf);
error1:
    return ret;

}

static void mp2p_uninit(Track *tr)
{
    if(tr->private_data) {
        g_slice_free(mp2p_priv, tr->private_data);
        tr->private_data = NULL;
    }
}

FNC_LIB_MEDIAPARSER(mp2p);


#endif
