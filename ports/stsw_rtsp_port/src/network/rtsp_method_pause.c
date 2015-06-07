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

/** @file
 * @brief Contains PAUSE method and reply handlers
 */


#include "feng.h"
#include "rtsp.h"
#include "rtp.h"
#include "media/demuxer.h"

#include <stdbool.h>

static void rtp_session_last_timestamp(gpointer session_gen, gpointer last_timestamp_gen) {
    RTP_session *session = (RTP_session*)session_gen;
    double *last_timestamp = (double*)last_timestamp_gen;
    if(session->last_timestamp > *last_timestamp) {
        *last_timestamp = session->last_timestamp;
    }
}


double rtsp_session_last_timestamp(RTSP_session *session) {
    double last_timestamp = 0;

    if( session != NULL && session->rtp_sessions != NULL) {
        g_slist_foreach(session->rtp_sessions, rtp_session_last_timestamp, &last_timestamp);
    }

    return last_timestamp;

}



/**
 *  Actually pause playing the media using mediathread
 *
 *  @param rtsp the client requesting pause
 */

void rtsp_do_pause(RTSP_Client *rtsp)
{
    RTSP_session *rtsp_sess = rtsp->session;
    /* Get the first range, so that we can record the pause point */
    RTSP_Range *range = g_queue_peek_head(rtsp_sess->play_requests) ; 

    if(rtsp_sess->resource->info->seekable){
        /* Jamken: if seekable, store the last position */
        range->begin_time = rtsp_session_last_timestamp(rtsp_sess); /* next 1ms to continue */ 
        range->seek = FALSE;
    }

    range->playback_time = -0.1;
    
    rtp_session_gslist_pause(rtsp_sess->rtp_sessions);
    
    
    rtsp_sess->started = 0;    
    if(rtsp_sess->fill_pool){
        g_thread_pool_free(rtsp_sess->fill_pool, true, true);
        rtsp_sess->fill_pool = NULL;
    }

 
    
    //pause resource
    r_pause(rtsp_sess->resource != NULL);

    ev_timer_stop(rtsp->srv->loop, &rtsp->ev_timeout);

    rtsp_sess->cur_state = RTSP_SERVER_READY;

    
}

/**
 * RTSP PAUSE method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 */
void RTSP_pause(RTSP_Client * rtsp, RTSP_Request *req)
{
    /* This is only valid in Playing state */
    if ( !rtsp_check_invalid_state(req, RTSP_SERVER_INIT) ||
         !rtsp_check_invalid_state(req, RTSP_SERVER_READY) )
        return;

    if ( !rtsp_request_check_url(req) )
        return;

    /** @todo we need to check if the client provided a Range
     *        header */
    rtsp_do_pause(rtsp);

    rtsp_quick_response(req, RTSP_Ok);
}
