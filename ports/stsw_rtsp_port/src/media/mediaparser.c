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

#include <string.h>

#include "mediaparser.h"
#include "demuxer.h"
#include "bufferqueue.h"
#include "fnc_log.h"

// global media parsers modules:
//extern MediaParser fnc_mediaparser_mpv;
//extern MediaParser fnc_mediaparser_mpa;
extern MediaParser fnc_mediaparser_h264;
extern MediaParser fnc_mediaparser_aac;
extern MediaParser fnc_mediaparser_mp4ves;
extern MediaParser fnc_mediaparser_vorbis;
extern MediaParser fnc_mediaparser_theora;
//extern MediaParser fnc_mediaparser_speex;
//extern MediaParser fnc_mediaparser_mp2t;
//extern MediaParser fnc_mediaparser_h263;
extern MediaParser fnc_mediaparser_amr;
extern MediaParser fnc_mediaparser_pcma;
extern MediaParser fnc_mediaparser_pcmu;
extern MediaParser fnc_mediaparser_mp2p;
extern MediaParser fnc_mediaparser_simple;
// static array containing all the available media parsers:
static MediaParser *media_parsers[] = {
    //&fnc_mediaparser_mpv,
    //&fnc_mediaparser_mpa,
    &fnc_mediaparser_h264,
    &fnc_mediaparser_aac,
    &fnc_mediaparser_mp4ves,
    &fnc_mediaparser_vorbis,
    &fnc_mediaparser_theora,
    //&fnc_mediaparser_speex,
    //&fnc_mediaparser_mp2t,
    //&fnc_mediaparser_h263,
    &fnc_mediaparser_amr,
    &fnc_mediaparser_pcma,
    &fnc_mediaparser_pcmu,
    &fnc_mediaparser_mp2p,
    NULL
};



MediaParser *mparser_find(const char *encoding_name)
{
    int i;

    for(i=0; media_parsers[i]; i++) {
        if ( !g_ascii_strcasecmp(encoding_name,
                                 media_parsers[i]->info->encoding_name) ) {
            fnc_log(FNC_LOG_DEBUG, "[MT] Found Media Parser for %s\n",
                    encoding_name);
            return media_parsers[i];
        }
    }
    /* Jmkn: if no find, use the default simple parser */

    fnc_log(FNC_LOG_DEBUG, "[MT] Media Parser for %s not found, use simple parser\n",
            encoding_name);
    return &fnc_mediaparser_simple;

}

/**
 *  Insert a rtp packet inside the track buffer queue
 *  @param tr track the packetized frames/samples belongs to
 *  @param presentation the actual packet presentation timestamp
 *         in fractional seconds, will be embedded in the rtp packet
 *  @param delivery the actual packet delivery timestamp
 *         in fractional seconds, will be used to calculate sending time
 *  @param duration the actual packet duration, a multiple of the frame/samples
 *  duration
 *  @param marker tell if we are handling a frame/sample fragment
 *  @param data actual packet data
 *  @param data_size actual packet data size
 */
void mparser_buffer_write(Track *tr,
                          double presentation,
                          double delivery,
                          double duration,
                          gboolean marker,
                          uint8_t *data, size_t data_size)
{

    if(tr != NULL && tr->producer != NULL &&
       bq_producer_consumer_num(tr->producer) > 0) {
 

    MParserBuffer *buffer = g_malloc(sizeof(MParserBuffer) + data_size);

    buffer->timestamp = presentation;
    buffer->delivery = delivery;
    buffer->duration = duration;
    buffer->marker = marker;
    buffer->data_size = data_size;

    memcpy(buffer->data, data, data_size);

    bq_producer_put(tr->producer, buffer);


    tr->parent->lastTimestamp = presentation;
    }

}
