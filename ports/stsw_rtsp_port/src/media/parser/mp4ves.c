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

#include "media/demuxer.h"
#include "media/mediaparser.h"
#include "media/mediaparser_module.h"
#include "fnc_log.h"

#ifdef TRISOS
#include "network/rtsp.h"
#endif


static const MediaParserInfo info = {
    "mp4v-es",
    MP_video
};

#define mp4ves_uninit NULL


#ifdef TRISOS


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

#endif


static int mp4ves_init(Track *track)
{
    char *config;
    int levelId;

    if ( (config = extradata2config(&track->properties)) == NULL )
        return ERR_PARSE;
#ifdef TRISOS
    if((levelId = getProfileLevelIdFromextradata(&track->properties)) == -1) {
        return ERR_PARSE;
    }

    track_add_sdp_field(track, fmtp,
                        g_strdup_printf("profile-level-id=%d;config=%s;",
                                        levelId, config));
#else
    track_add_sdp_field(track, fmtp,
                        g_strdup_printf("profile-level-id=1;config=%s;",
                                        config));
#endif

    track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("MP4V-ES/%d",
                                         track->properties.clock_rate));

    g_free(config);

    return ERR_NOERROR;
}

static int mp4ves_parse(Track *tr, uint8_t *data, size_t len)
{
    int32_t offset, rem = len;

#if 0
    uint32_t start_code = 0, ptr = 0;
    while ((ptr = (uint32_t) find_start_code(data + ptr, data + len, &start_code)) < (uint32_t) data + len ) {
        ptr -= (uint32_t) data;
        fnc_log(FNC_LOG_DEBUG, "[mp4v] start code offset %d", ptr);
    }
    fnc_log(FNC_LOG_DEBUG, "[mp4v]no more start codes");
#endif
#ifdef TRISOS
    unsigned int scale = 1;
    int onlyKeyFrame = 0;

    tr->packetTotalNum++;

    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        scale =  (((RTSP_session *)tr->parent->rtsp_sess)->scale <= 1.0)?
            ((unsigned int)1): ((unsigned int)((RTSP_session *)tr->parent->rtsp_sess)->scale);
        onlyKeyFrame = ((RTSP_session *)tr->parent->rtsp_sess)->onlyKeyFrame;
    }

    if(onlyKeyFrame == 0 ||
       isMp4vKeyFrame(data, len) ) {


#endif




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
#ifdef TRISOS
    }
#endif


    return ERR_NOERROR;
}

FNC_LIB_MEDIAPARSER(mp4ves);
