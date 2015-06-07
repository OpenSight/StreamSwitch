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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * */

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
    "simple",
    MP_audio
};

static int simple_init(Track *track)
{
    char encodingParamsPart[21];
    memset(encodingParamsPart, 0, sizeof(encodingParamsPart));
    if(track->properties.media_type == MP_audio){
        if(track->properties.audio_channels > 1){
            sprintf(encodingParamsPart, "/%d", 
                    track->properties.audio_channels);
        }
    }
    
	track_add_sdp_field(track, rtpmap,
                        g_strdup_printf ("%s/%d%s",
                                         track->properties.encoding_name,
                                         track->properties.clock_rate, 
                                         encodingParamsPart));
    return ERR_NOERROR;
}

static int simple_parse(Track *tr, uint8_t *data, size_t len)
{
    int32_t offset = 0, rem = len;


    if (DEFAULT_MTU >= len) {
        mparser_buffer_write(tr,
                             tr->properties.pts,
                             tr->properties.dts,
                             tr->properties.frame_duration,
                             1,
                             data, len);
        fnc_log(FNC_LOG_VERBOSE, "[simple] no frags");
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
            fnc_log(FNC_LOG_VERBOSE, "[simple] frags");
        } while (rem >= 0);
    }
    fnc_log(FNC_LOG_VERBOSE, "[simple] Frame completed");
    
    
    return ERR_NOERROR;

}

#define simple_uninit NULL
#define simple_reset NULL


FNC_LIB_MEDIAPARSER(simple);


