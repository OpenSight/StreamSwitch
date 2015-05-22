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
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "media/demuxer.h"
#include "media/mediaparser.h"
#include "media/mediaparser_module.h"
#include "feng_utils.h"
#include "fnc_log.h"

#include "network/rtsp.h"


static const MediaParserInfo info = {
    "pcmu",
    MP_audio
};

static int pcmu_init(ATTR_UNUSED Track *track)
{
    track->properties.clock_rate = 8000;
	 track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("PCMU/%d",
                                         track->properties.clock_rate));
    return ERR_NOERROR;
}

static int pcmu_parse(Track *tr, uint8_t *data, size_t len)
{

    unsigned i;
    unsigned sliceBytes = 320;
    unsigned sliceLen = 0;
    unsigned sliceNum = 0;
    double byteDuration;
    unsigned int scale = 1;
    uint8_t *temp_data = NULL;
    size_t temp_len = 0;


    if(tr->parent != NULL && tr->parent->rtsp_sess != NULL) {
        scale =  (((RTSP_session *)tr->parent->rtsp_sess)->scale <= 1.0)?
                ((unsigned int)1): ((unsigned int)((RTSP_session *)tr->parent->rtsp_sess)->scale);
    }

    if(scale == 1 ||
       ((RTSP_session *)tr->parent->rtsp_sess)->download != 0 || 
       ((RTSP_session *)tr->parent->rtsp_sess)->disableRateControl != 0 ||
       ((RTSP_session *)tr->parent->rtsp_sess)->simpleAudioSkip == 0){
        /* don't abstract */
    }else{
        temp_len = len / scale;
        temp_data = (uint8_t *)g_malloc(temp_len);
        if(temp_data == NULL) {
            return RESOURCE_NOT_PARSEABLE;
        }
        for(i=0;i<temp_len;i++) {
            temp_data[i] = data[i*scale];
        }

        len = temp_len;
        data = temp_data;
    }


    sliceNum = (len + sliceBytes - 1) / sliceBytes;
    byteDuration = tr->properties.frame_duration / len ;


    for(i=0;i<sliceNum;i++) {

        
        sliceLen = ((len - i * sliceBytes) >= sliceBytes)?sliceBytes:(len - i * sliceBytes);
        mparser_buffer_write(tr,
                             tr->properties.pts + i * sliceBytes * byteDuration,
                             tr->properties.dts + i * sliceBytes * byteDuration,
                             sliceLen * byteDuration,
                             1,
                             data + i * sliceBytes, sliceLen);
        fnc_log(FNC_LOG_VERBOSE, "[pcmu] slice size %d", sliceLen);

    }

    if(temp_data != NULL) {
        g_free(temp_data);

    }

    return ERR_NOERROR;
}

#define pcmu_uninit NULL

FNC_LIB_MEDIAPARSER(pcmu);
#endif
