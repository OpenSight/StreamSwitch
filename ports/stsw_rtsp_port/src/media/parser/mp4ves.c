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

#include "media/demuxer.h"
#include "media/mediaparser.h"
#include "media/mediaparser_module.h"
#include "fnc_log.h"
#include "network/rtsp.h"



static const MediaParserInfo info = {
    "mp4v-es",
    MP_video
};


typedef struct {
    int gov_start; // if the parser has seen GOV start flag, gov_start would be set to 1
} mp4v_priv;

#define GROUP_VOP_START_CODE      0xB3
#define VOP_START_CODE            0xB6
#define VISUAL_OBJECT_SEQUENCE_START_CODE      0xB0
#define VOP_CODING_TYPE_I         0
#define VOP_CODING_TYPE_P         1
#define VOP_CODING_TYPE_B         2
#define VOP_CODING_TYPE_S         3

int isMp4vKeyFrame(uint8_t * frameSource, size_t frameSize)
{
    unsigned i;
    unsigned char vop_coding_type;

    if(frameSource == NULL || frameSize < 4) {
        return 0;
    }
    
    for (i = 3; i < frameSize; ++i) {
        if (frameSource[i-1] == 1 && frameSource[i-2] == 0 && frameSource[i-3] == 0) {
            switch(frameSource[i]) {
            case VISUAL_OBJECT_SEQUENCE_START_CODE:
            case GROUP_VOP_START_CODE:
                return 1;
                break;
            case VOP_START_CODE:
                if(i+1 < frameSize ) {
                    vop_coding_type = frameSource[i+1] >> 6;
                    if(vop_coding_type == VOP_CODING_TYPE_I) {
                        return 1;
                    }else{
                        return 0;
                    }
                }
                break;
            }

        }
    }    
        
    return 0;    

}


static int mp4ves_init(Track *track)
{
    char *config;
    int levelId;

    if ( (config = extradata2config(&track->properties)) == NULL )
        return ERR_PARSE;

    if((levelId = getProfileLevelIdFromextradata(&track->properties)) == -1) {
        return ERR_PARSE;
    }

    track_add_sdp_field(track, fmtp,
                        g_strdup_printf("profile-level-id=%d;config=%s;",
                                        levelId, config));

    track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("MP4V-ES/%d",
                                         track->properties.clock_rate));

    g_free(config);
    
    track->private_data = g_slice_new0(h264_priv);

    return ERR_NOERROR;
}

static int mp4ves_parse(Track *tr, uint8_t *data, size_t len)
{
    int32_t offset, rem = len;

    unsigned int scale = 1;
    int onlyKeyFrame = 0;
    mp4v_priv *priv = tr->private_data;
    int is_key_frame = isMp4vKeyFrame(data, len);
    
    if(is_key_frame){
        priv->gov_start = 1;
    }
    if(priv->gov_start == 0){
        /* if the parser does not encounter the GOV, just drop the frame */
        return;
    }    

    tr->packetTotalNum++;

    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        scale =  (((RTSP_session *)tr->parent->rtsp_sess)->scale <= 1.0)?
            ((unsigned int)1): ((unsigned int)((RTSP_session *)tr->parent->rtsp_sess)->scale);
        onlyKeyFrame = ((RTSP_session *)tr->parent->rtsp_sess)->onlyKeyFrame;
    }

    if(onlyKeyFrame == 0 ||
       is_key_frame ) {


        if (DEFAULT_MTU >= len) {
            mparser_buffer_write(tr,
                                 tr->properties.pts,
                                 tr->properties.dts,
                                 tr->properties.frame_duration,
                                 1,
                                 data, len);
            fnc_log(FNC_LOG_VERBOSE, "[mp4v] no frags");
        } else {
            do {
                offset = len - rem;
                mparser_buffer_write(tr,
                                     tr->properties.pts,
                                     tr->properties.dts,
                                     tr->properties.frame_duration,
                                     (rem <= DEFAULT_MTU),
                                     data + offset, MIN(rem, DEFAULT_MTU));
                rem -= DEFAULT_MTU;
                fnc_log(FNC_LOG_VERBOSE, "[mp4v] frags");
            } while (rem >= 0);
        }
        fnc_log(FNC_LOG_VERBOSE, "[mp4v]Frame completed");

    }



    return ERR_NOERROR;
}
static void mp4ves_uninit(Track *tr)
{
    g_slice_free(mp4v_priv, tr->private_data);
}

static void mp4ves_reset(Track *tr)
{
    mp4v_priv *priv = tr->private_data; 
    priv->gov_start = 0;
}


FNC_LIB_MEDIAPARSER(mp4ves);
