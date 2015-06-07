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

#include "fnc_log.h"
#include "media/demuxer.h"
#include "media/mediaparser.h"
#include "media/mediaparser_module.h"

#include "network/rtsp.h"


static const MediaParserInfo info = {
    "H264",
    MP_video
};

typedef struct {
    int is_avc;
    unsigned int nal_length_size; // used in avc
} h264_priv;

/* Generic Nal header
 *
 *  +---------------+
 *  |0|1|2|3|4|5|6|7|
 *  +-+-+-+-+-+-+-+-+
 *  |F|NRI|  Type   |
 *  +---------------+
 *
 */
/*  FU header
 *  +---------------+
 *  |0|1|2|3|4|5|6|7|
 *  +-+-+-+-+-+-+-+-+
 *  |S|E|R|  Type   |
 *  +---------------+
 */

static void frag_fu_a(uint8_t *nal, int fragsize, int mtu,
                      Track *tr)
{
    int start = 1, fraglen;
    uint8_t fu_header, buf[mtu];
    fnc_log(FNC_LOG_VERBOSE, "[h264] frags");
//                p = data + index;
    buf[0] = (nal[0] & 0xe0) | 28; // fu_indicator
    fu_header = nal[0] & 0x1f;
    nal++;
    fragsize--;
    while(fragsize>0) {
        buf[1] = fu_header;
        if (start) {
            start = 0;
            buf[1] = fu_header | (1<<7);
        }
        fraglen = MIN(mtu-2, fragsize);
        if (fraglen == fragsize) {
            buf[1] = fu_header | (1<<6);
        }
        memcpy(buf + 2, nal, fraglen);
        fnc_log(FNC_LOG_VERBOSE, "[h264] Frag %d %d",buf[0], buf[1]);
        mparser_buffer_write(tr,
                             tr->properties.pts,
                             tr->properties.dts,
                             tr->properties.frame_duration,
                             (fragsize<=fraglen),
                             buf, fraglen + 2);
        fragsize -= fraglen;
        nal      += fraglen;
    }
}

#define RB16(x) ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])

static char *encode_avc1_header(uint8_t *p, unsigned int len, int packet_mode)
{
    int i, cnt, nalsize;
    uint8_t *q = p;
    char *sprop = NULL;
    cnt = *(p+5) & 0x1f; // Number of sps
    p += 6;

    for (i = 0; i < cnt; i++) {
        if (p > q + len)
            goto err_alloc;
        nalsize = RB16(p); //buf_size
        p += 2;
        fnc_log(FNC_LOG_VERBOSE, "[h264] nalsize %d", nalsize);
        if (i == 0) {
            gchar *out = g_strdup_printf("profile-level-id=%02x%02x%02x; "
                                  "packetization-mode=%d; ",
                                  p[1], p[2], p[3], packet_mode);

	    gchar *buf = g_base64_encode(p, nalsize);
            sprop = g_strdup_printf("%ssprop-parameter-sets=%s", out, buf);
	    g_free(buf);
            g_free(out);
        } else {
            gchar *buf = g_base64_encode(p, nalsize);
            gchar *out = g_strdup_printf("%s,%s", sprop, buf);
            g_free(sprop);
	    g_free(buf);
            sprop = out;
        }
        p += nalsize;
    }
    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    fnc_log(FNC_LOG_VERBOSE, "[h264] pps %d", cnt);

    for (i = 0; i < cnt; i++) {
        gchar *out, *buf;

        if (p > q + len)
            goto err_alloc;
        nalsize = RB16(p);
        p += 2;
        fnc_log(FNC_LOG_VERBOSE, "[h264] nalsize %d", nalsize);
	buf = g_base64_encode(p, nalsize);
        out = g_strdup_printf("%s,%s",sprop, buf);
        g_free(sprop);
	g_free(buf);
        sprop = out;
        p += nalsize;
    }

    return sprop;

    err_alloc:
        if (sprop) g_free(sprop);
    return NULL;
}

static char *encode_header(uint8_t *p, unsigned int len, int packet_mode)
{
    uint8_t *q, *end = p + len;
    char *sprop = NULL, *out, *buf = NULL;

    for (q = p; q < end - 4; q++) {
        if (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1) {
            break;
        }
    }

    if (q >= end - 4)
        return NULL;

    p = q; // sps start;

    for (; q < end - 4; q++) {
        if (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1) {
            /* sps end */
            break;
        }
        
    }
    if(q >= end - 4){
        q = end; /* last one */
    }


    p += 4;
    out = g_strdup_printf("profile-level-id=%02x%02x%02x; "
                          "packetization-mode=%d; ",
                            p[1], p[2], p[3], packet_mode);

    buf = g_base64_encode(p, q-p);

    sprop = g_strdup_printf("%ssprop-parameter-sets=%s", out, buf);
    g_free(out);
    g_free(buf);
    p = q;

    while (p < end - 4) {
        //seek to the next startcode [0 0 1]
        for (q = (p + 4); q < end - 4; q++) {
            if (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1) {
                /* sps end */
                break;
            }
            
        }
        if(q >= end - 4){
            q = end; /* last one */
        }
        p += 4;
        buf = g_base64_encode(p, q - p);
        out = g_strdup_printf("%s,%s",sprop, buf);
        g_free(sprop);
        g_free(buf);
        sprop = out;
        p = q;
    }

    return sprop;
}

#define FU_A 1

static int h264_init(Track *track)
{
    h264_priv *priv;
    char *sprop = NULL;
    int err = ERR_ALLOC;

    if (track->properties.extradata_len == 0) {
        fnc_log(FNC_LOG_WARN, "[h264] No Extradata, unsupported\n");
        return ERR_UNSUPPORTED_PT;
    }

    priv = g_slice_new0(h264_priv);

    if(track->properties.extradata[0] == 1) {
        if (track->properties.extradata_len < 7) goto err_alloc;
        priv->nal_length_size = (track->properties.extradata[4]&0x03)+1;
        priv->is_avc = 1;
        sprop = encode_avc1_header(track->properties.extradata,
                                   track->properties.extradata_len, FU_A);
        if (sprop == NULL) goto err_alloc;
    } else {
        sprop = encode_header(track->properties.extradata,
                              track->properties.extradata_len, FU_A);
        if (sprop == NULL) goto err_alloc;
    }

    track_add_sdp_field(track, fmtp, sprop);

    track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("H264/%d",track->properties.clock_rate));

    track->private_data = priv;

    return ERR_NOERROR;

 err_alloc:
    g_slice_free(h264_priv, priv);
    return err;
}
/* send a h264 nal to bufferqueue */
/* data - contains a h264 nal */
static void h264_send_nal(Track *tr, uint8_t *data, size_t len)
{
    uint8_t nal_unit_type;
    unsigned int scale = 1;
    int onlyKeyFrame = 0;    
    
    tr->packetTotalNum++;


    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        scale =  (((RTSP_session *)tr->parent->rtsp_sess)->scale <= 1.0)?
                    ((unsigned int)1): ((unsigned int)((RTSP_session *)tr->parent->rtsp_sess)->scale);
        onlyKeyFrame = ((RTSP_session *)tr->parent->rtsp_sess)->onlyKeyFrame;
    }
    
    
    nal_unit_type = data[0] & 0x1F;
    
    if(onlyKeyFrame == 0 ||
       /*    tr->packetTotalNum % scale == 0 || */
       nal_unit_type >= 5) {
                
          
        if (DEFAULT_MTU >= len) {
                mparser_buffer_write(tr,
                                     tr->properties.pts,
                                     tr->properties.dts,
                                     tr->properties.frame_duration,
                                     1,
                                     data, len);
                fnc_log(FNC_LOG_VERBOSE, "[h264] single NAL");
        } else {
                // single NAL, to be fragmented, FU-A;
                frag_fu_a(data, len, DEFAULT_MTU, tr);
        } 


    }else if ( tr->packetTotalNum % scale == 0  ){
        /* Jamken: send a empty nal to feed the player, 
          VLC need get a frame to continue */
                    uint8_t  fakeNal[] = {0x09, 0x30};
                    size_t fakeNalLen = sizeof(fakeNal);
                    
                    


            mparser_buffer_write(tr,
                                 tr->properties.pts,
                                 tr->properties.dts,
                                 tr->properties.frame_duration,
                                 1,
                                 fakeNal, fakeNalLen);
            fnc_log(FNC_LOG_VERBOSE, "[h264] single NAL");
    }
 
}

// h264 has provisions for
//  - collating NALS
//  - fragmenting
//  - feed a single NAL as is.

static int h264_parse(Track *tr, uint8_t *data, size_t len)
{
    h264_priv *priv = tr->private_data;
//    double nal_time; // see page 9 and 7.4.1.2
    size_t nalsize = 0, index = 0;
    uint8_t *p, *q;

    if (priv->is_avc) {
        while (1) {
            unsigned int i;

            if(index >= len) break;
            //get the nal size


            nalsize = 0;
            for(i = 0; i < priv->nal_length_size; i++)
                nalsize = (nalsize << 8) | data[index++];
            if(nalsize <= 1 || nalsize > len) {
                if(nalsize == 1) {
                    index++;
                    continue;
                } else {
                    fnc_log(FNC_LOG_VERBOSE, "[h264] AVC: nal size %d", nalsize);
                    break;
                }
            }
           
            h264_send_nal(tr, data + index, nalsize);

            index += nalsize;
        }
    } else {
        p = data;
        if(p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3]  == 1){
            /*it's a ES stream of h264 */
            while (1) {
               //seek to the next startcode [0 0 0 1]
                for (q = (p+4); q<(data+len-4);q++) {
                    if (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1) {
                        break;
                    }
                }

                if (q >= data + len - 4) {
                    /* no next start_code, this is the last nal */
                    break;
                }
                
                h264_send_nal(tr,  p + 4, (q - p - 4));
                p = q;

            }
            // last NAL
            fnc_log(FNC_LOG_VERBOSE, "[h264] last NAL %d",p[0]&0x1f);
            
            h264_send_nal(tr,  p + 4, (data + len - p - 4));
            
        }else{
            /* this is directry a nal */      
            h264_send_nal(tr, data, len);
            
        }
    }

    fnc_log(FNC_LOG_VERBOSE, "[h264] Frame completed");
    return ERR_NOERROR;
}

static void h264_uninit(Track *tr)
{
    g_slice_free(h264_priv, tr->private_data);
}

#define h264_reset NULL

FNC_LIB_MEDIAPARSER(h264);

