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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "feng.h"
#include "demuxer.h"
#include "mediaparser.h"
#include "fnc_log.h"



static void free_sdp_field(sdp_field *sdp,
                           void *unused)
{
    if (!sdp)
        return;

    g_free(sdp->field);
    g_slice_free(sdp_field, sdp);
}

static void sdp_fields_free(GSList *fields)
{
    if ( fields == NULL )
        return;

    g_slist_foreach(fields, (GFunc)free_sdp_field, NULL);
    g_slist_free(fields);
}

/**
 * @brief Frees the resources of a Track object
 *
 * @param element Track to free
 * @param user_data Unused, for compatibility with g_list_foreach().
 */
void free_track(gpointer element,
                gpointer user_data)
{
    Track *track = (Track*)element;

    if (!track)
        return;

    if((track->producer)&&(track->producer_freed == 0))
    {

        bq_producer_unref(track->producer);
    
    }
    track->producer = NULL;


    g_free(track->info->mrl);
    
    g_slice_free(TrackInfo, track->info);

    sdp_fields_free(track->sdp_fields);

    if ( track->parser && track->parser->uninit )
        track->parser->uninit(track);

    g_slice_free(Track, track);
}

/**
 * @brief reset the internal processing state of a Track object
 * 
 * by now, reset the producer queue and the codec parser used by this track,
 *
 * @param element Track to free
 * @param user_data Unused, for compatibility with g_list_foreach().
 */
void reset_track(gpointer element, gpointer user_data)
{
    Track *track = (Track*)element;

    if (!track)
        return;
    
    if(track->producer){
        bq_producer_reset_queue(track->producer);
    }
    
    if(track->parser && track->parser->reset){
        track->parser->reset(track);
    }
}


#define ADD_TRACK_ERROR(level, ...) \
    { \
        fnc_log(level, __VA_ARGS__); \
        goto error; \
    }

/*! Add track to resource tree.  This function adds a new track data struct to
 * resource tree. It used by specific demuxer function in order to obtain the
 * struct to fill.
 * @param r pointer to resource.
 * @return pointer to newly allocated track struct.
 * */

Track *add_track(Resource *r, TrackInfo *info, MediaProperties *prop_hints)
{
    Track *t;
    // TODO: search first of all in exclusive tracks

    if(r->num_tracks>=MAX_TRACKS)
        return NULL;

    t = g_slice_new0(Track);

    t->packetTotalNum = 0;

    t->parent = r;

    t->info = g_slice_new0(TrackInfo);
    memcpy(t->info, info, sizeof(TrackInfo));

    memcpy(&t->properties, prop_hints, sizeof(MediaProperties));

    switch (t->properties.media_source) {
    case MS_live:    
    case MS_stored:
        if( !(t->producer = bq_producer_new(g_free, NULL)) )
            ADD_TRACK_ERROR(FNC_LOG_FATAL, "Memory allocation problems\n");
        if ( !(t->parser = mparser_find(t->properties.encoding_name)) )
            ADD_TRACK_ERROR(FNC_LOG_FATAL, "Could not find a valid parser\n");
        if (t->parser->init(t))
            ADD_TRACK_ERROR(FNC_LOG_FATAL, "Could not initialize parser for %s\n",
                            t->properties.encoding_name);

        //t->properties.media_type = t->parser->info->media_type;
        break;
    /* Jamken: rtsp port use fork() model, so that it's impossible to share 
     * producer queue
     */ 
#if 0
    case MS_live:
        if( !(t->producer = bq_producer_new(g_free, t->info->mrl)) )
            ADD_TRACK_ERROR(FNC_LOG_FATAL, "Memory allocation problems\n");
        break;
#endif

    default:
        ADD_TRACK_ERROR(FNC_LOG_FATAL, "Media source not supported!");
        break;
    }

    r->tracks = g_list_append(r->tracks, t);
    r->num_tracks++;

    return t;

 error:
    free_track(t, NULL);
    return NULL;
}
#undef ADD_TRACK_ERROR

/**
 * @brief Append an SDP field to the track
 *
 * @param track The track to add the fields to
 * @param type The type of the field to add
 * @param value The value of the field to add (already duped)
 */
void track_add_sdp_field(Track *track, sdp_field_type type, char *value)
{
    sdp_field *field = g_slice_new(sdp_field);
    field->type = type;
    field->field = value;

    track->sdp_fields = g_slist_prepend(track->sdp_fields, field);
}



