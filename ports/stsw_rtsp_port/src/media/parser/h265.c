/**
 * This file is part of stsw_rtsp_port, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2015  OpenSight team (www.opensight.cn)
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
    "H265",
    MP_video
};

typedef struct {
    int gov_start; // if the parser has seen GOV start flag, gov_start would be set to 1
} h265_priv;


/* this parser is developed based on 
 * draft-ietf-payload-rtp-h265-13
 * */

/* Generic Nal header
 *
 *  +---------------+---------------+
 *  |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |F|   Type    |  LayerId  | TID |
 *  +-------------+-----------------+
 *
 */
/*  FU header
 * +---------------+
 * |0|1|2|3|4|5|6|7|
 * +-+-+-+-+-+-+-+-+
 * |S|E|  FuType   |
 * +---------------+
 */

static void frag_fu_a(uint8_t *nal, int fragsize, int mtu,
                      Track *tr)
{
    int start = 1, fraglen;
    uint8_t fu_header, buf[mtu];
    
    uint8_t nal_unit_type = (nal[0]&0x7E)>>1;
    fnc_log(FNC_LOG_VERBOSE, "[h265] frags");
    buf[0] = (nal[0] & 0x81) | (49<<1); // Payload header (1st byte)
    buf[1] = nal[1]; // Payload header (2nd byte)    
    fu_header = nal_unit_type;
    
    nal += 2;
    fragsize -= 2;
    while(fragsize>0) {
        buf[2] = fu_header;
        if (start) {
            start = 0;
            buf[2] = fu_header | 0x80;
        }
        fraglen = MIN(mtu-3, fragsize);
        if (fraglen == fragsize) {
            buf[2] = fu_header | 0x40;
        }
        memcpy(buf + 3, nal, fraglen);
        fnc_log(FNC_LOG_VERBOSE, "[h265] Frag %02x %02x %02x",buf[0], buf[1], buf[2]);
        mparser_buffer_write(tr,
                             tr->properties.pts,
                             tr->properties.dts,
                             tr->properties.frame_duration,
                             (fragsize<=fraglen),
                             buf, fraglen + 3);
        fragsize -= fraglen;
        nal      += fraglen;
    }
}


static char *encode_header(uint8_t *data, unsigned int len)
{
    uint8_t *q = NULL;
    uint8_t *p = NULL;
    char *sprop = NULL;
    uint8_t nal_unit_type;
    uint8_t *vps = NULL;
    int vps_size = 0;
    uint8_t *sps = NULL;
    int sps_size = 0;    
    uint8_t *pps = NULL;
    int pps_size = 0;   
    int nal_size = 0;
    uint8_t* profileTierLevelHeaderBytes = NULL;
    unsigned profileSpace  = 0;
    unsigned profileId = 0;
    unsigned tierFlag = 0;
    unsigned levelId = 0;
    uint8_t* interop_constraints = NULL;
    char interopConstraintsStr[100];    
    char* sprop_vps = NULL;
    char* sprop_sps = NULL;
    char* sprop_pps = NULL;

    
/*    
        fnc_log(FNC_LOG_DEBUG, "[h264] header len %d",
                        len);
*/
    if(len <= 4){
        return NULL;
    }

    p = data;
    if (p[0] != 0 || p[1] != 0 || p[2] != 0 || p[3] != 1) {
        return NULL;
    }
    
    //find vps, sps, pps
    while (p < (data + len - 4)) {
        for (q = (p+4); q<(data+len-4);q++) {
            if (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 1) {
                break;
            }
        }

        if (q >= (data + len - 4)) {
            /* no next start_code, this is the last nal */
            nal_size = data + len - p - 4;
        }else{
            nal_size = q - p - 4;
        }
                
        //h264_send_nal(tr,  p + 4, (q - p - 4));
        //get a nal(p+4, q - p -4);
        nal_unit_type = (p[4] & 0x7E) >> 1;
        
        switch(nal_unit_type){
        case 32:
            vps = p + 4;
            vps_size = nal_size;
            break;
        case 33:
            sps = p + 4;
            sps_size = nal_size;
            break;
        case 34:
            pps = p + 4;
            pps_size = nal_size;
            break;            
        }        
        p = q;
    }//while

    if(vps == NULL || sps == NULL || pps == NULL){
        fnc_log(FNC_LOG_DEBUG, "[h265] extra_data not include vps, sps, pps");    
        return NULL;
    }
    
    
    
    // Set up the "a=fmtp:" SDP line for this stream.

    if (vps_size < 6/*'profile_tier_level' offset*/ + 12/*num 'profile_tier_level' bytes*/) {
        // Bad VPS size => assume our source isn't ready
        fnc_log(FNC_LOG_DEBUG, "[h265] vps size invalid");

        return NULL;
    }
    profileTierLevelHeaderBytes = &vps[6];
    profileSpace  = profileTierLevelHeaderBytes[0]>>6; // general_profile_space
    profileId = profileTierLevelHeaderBytes[0]&0x1F; // general_profile_idc
    tierFlag = (profileTierLevelHeaderBytes[0]>>5)&0x1; // general_tier_flag
    levelId = profileTierLevelHeaderBytes[11]; // general_level_idc
    interop_constraints = &profileTierLevelHeaderBytes[5];
    sprintf(interopConstraintsStr, "%02X%02X%02X%02X%02X%02X", 
        interop_constraints[0], interop_constraints[1], interop_constraints[2],
        interop_constraints[3], interop_constraints[4], interop_constraints[5]);
  
    
    
    sprop_vps = g_base64_encode(vps, vps_size);
    sprop_sps = g_base64_encode(sps, sps_size);
    sprop_pps = g_base64_encode(pps, pps_size);
   
    sprop = g_strdup_printf("profile-space=%u"
                            ";profile-id=%u"
                            ";tier-flag=%u"
                            ";level-id=%u"
                            ";interop-constraints=%s"
                            ";sprop-vps=%s"
                            ";sprop-sps=%s"
                            ";sprop-pps=%s", 
                            profileSpace,
                            profileId,
                            tierFlag,
                            levelId,
                            interopConstraintsStr,
                            sprop_vps,
                            sprop_sps,
                            sprop_pps);
    g_free(sprop_vps);
    g_free(sprop_sps);
    g_free(sprop_pps);

    return sprop;
}

static int h265_init(Track *track)
{
    h265_priv *priv;
    char *sprop = NULL;
    int err = ERR_ALLOC;

/*
    if (track->properties.extradata_len == 0) {
        fnc_log(FNC_LOG_WARN, "[h265] No Extradata, unsupported\n");
        return ERR_UNSUPPORTED_PT;
    }
*/
    priv = g_slice_new0(h265_priv);

    if(track->properties.extradata_len != 0){
        sprop = encode_header(track->properties.extradata,
                                track->properties.extradata_len, FU_A);
        if (sprop == NULL) goto err_alloc;


        track_add_sdp_field(track, fmtp, sprop);
        
    }

    track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("H265/%d",track->properties.clock_rate));

    track->private_data = priv;

    return ERR_NOERROR;

 err_alloc:
    g_slice_free(h265_priv, priv);
    return err;
}
/* send a h265 nal to bufferqueue */
/* data - contains a h265 nal */
static void h265_send_nal(Track *tr, uint8_t *data, size_t len)
{
    uint8_t nal_unit_type;
    unsigned int scale = 1;
    int onlyKeyFrame = 0;  
    h264_priv *priv = tr->private_data;
    
    
    nal_unit_type = (data[0] & 0x7E) >> 1;
    if(nal_unit_type == 32 || 
       nal_unit_type == 33 ||
       nal_unit_type == 19 || 
       nal_unit_type == 20){
        /* if this nal is VPS/SPS/IDR, a new gov is begin */
        priv->gov_start = 1;
    }
    if(priv->gov_start == 0){
        /* if the parser has not yet encounter the GOV start flag, just drop the frame */
        return;
    }
    
    tr->packetTotalNum++;


    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        scale =  (((RTSP_session *)tr->parent->rtsp_sess)->scale <= 1.0)?
                    ((unsigned int)1): ((unsigned int)((RTSP_session *)tr->parent->rtsp_sess)->scale);
        onlyKeyFrame = ((RTSP_session *)tr->parent->rtsp_sess)->onlyKeyFrame;
    }
    
    
    if(onlyKeyFrame == 0 ||
       /*    tr->packetTotalNum % scale == 0 || */
       nal_unit_type >= 16) {
                
          
        if (DEFAULT_MTU >= len) {
                mparser_buffer_write(tr,
                                     tr->properties.pts,
                                     tr->properties.dts,
                                     tr->properties.frame_duration,
                                     1,
                                     data, len);
                fnc_log(FNC_LOG_VERBOSE, "[h265] single NAL");
        } else {
                // single NAL, to be fragmented, FU-A;
                frag_fu_a(data, len, DEFAULT_MTU, tr);
        } 

    }
 
}

// h265 has provisions for
//  - collating NALS
//  - fragmenting
//  - feed a single NAL as is.

static int h265_parse(Track *tr, uint8_t *data, size_t len)
{
    h264_priv *priv = tr->private_data;
    size_t nalsize = 0, index = 0;
    uint8_t *p, *q;


    p = data;
    if(p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3]  == 1){
        /* it's a ES stream of h264 with start_code */
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
                
            h265_send_nal(tr,  p + 4, (q - p - 4));
            p = q;

        }
        // last NAL
        fnc_log(FNC_LOG_VERBOSE, "[h264] last NAL %d",p[0]&0x1f);
            
        h265_send_nal(tr,  p + 4, (data + len - p - 4));
            
    }else{
            /* this is directry a nal */      
        h265_send_nal(tr, data, len);
            
    }


    fnc_log(FNC_LOG_VERBOSE, "[h265] Frame completed");
    return ERR_NOERROR;
}

static void h265_uninit(Track *tr)
{
    g_slice_free(h265_priv, tr->private_data);
}


static void h265_reset(Track *tr)
{
    h265_priv *priv = tr->private_data; 
    priv->gov_start = 0;
}


FNC_LIB_MEDIAPARSER(h265);

