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
 *  @brief Contains PLAY method and reply handlers
 */

#include <glib.h>
#include <math.h>
#include <stdbool.h>
#include <netembryo/url.h>

#include <liberis/headers.h>

#include "feng.h"
#include "rtsp.h"
#include "rtp.h"
#include "fnc_log.h"
#include "ragel_parsers.h"
#include "media/demuxer.h"

/**
 * Actually starts playing the media using mediathread
 *
 * @param rtsp_sess the session for which to start playing
 *
 * @retval RTSP_Ok Operation successful
 * @retval RTSP_InvalidRange Seek couldn't be completed
 */
static RTSP_ResponseCode do_play(RTSP_session * rtsp_sess)
{
    RTSP_Range *range = g_queue_peek_head(rtsp_sess->play_requests);

    /* Don't try to seek if the source is not seekable;
     * parse_range_header() would have already ensured the range is
     * valid for the resource, and in particular ensured that if the
     * resource is not seekable we only have the “0-” range selected.
     */
    /* For seekable resource, if client not give a Range header, 
     * we don't seek so that it play from the last position
     */
    if ( rtsp_sess->resource->info->seekable && range->seek){
        if(r_seek(rtsp_sess->resource, range->begin_time) ){
            return RTSP_InvalidRange; 
        }
    }


    rtsp_sess->cur_state = RTSP_SERVER_PLAYING;

    if ( rtsp_sess->rtp_sessions &&
         ((RTP_session*)(rtsp_sess->rtp_sessions->data))->multicast )
        return RTSP_Ok;

    //start the resource
    if(rtsp_sess->resource != NULL){
        if(r_start(rtsp_sess->resource)){
            return RTSP_InternalServerError;
        }
    }else{
        return RTSP_InvalidMethodInState;
    }



    rtsp_sess->started = 1;



    fnc_log(FNC_LOG_VERBOSE, "[%f] resuming with parameters %f %f %f\n",
            ev_now(rtsp_sess->srv->loop),
            range->begin_time, range->end_time, range->playback_time);
    
    //create the fill pool
    if(rtsp_sess->resource->model == MM_PULL){
        rtsp_sess->fill_pool = g_thread_pool_new(rtp_session_fill_cb, rtsp_sess,
                                           1, true, NULL);        
    }    
        
    //resume all rtp session
    rtp_session_gslist_resume(rtsp_sess->rtp_sessions, range);
    
    return RTSP_Ok;
}

static void rtp_session_send_play_reply(gpointer element, gpointer user_data)
{
  GString *str = (GString *)user_data;

  RTP_session *p = (RTP_session *)element;
  Track *t = p->track;

  g_string_append_printf(str, "url=%s;seq=%u;rtptime=%u",
                         p->uri,
                         p->start_seq, 
                         p->start_rtptime);
/*
  if (t->properties.media_source != MS_live)
    g_string_append_printf(str,
			   ";rtptime=%u", p->start_rtptime);
*/
  g_string_append(str, ",");
}

/**
 * @brief Sends the reply for the play method
 *
 * @param req The Request to respond to.
 * @param rtsp_session the session for which to generate the reply
 * @param args The arguments to use to play
 *
 * @todo The args parameter should probably replaced by two begin_time
 *       and end_time parameters, since those are the only values used
 *       out of the structure.
 */
static void send_play_reply(RTSP_Request *req,
                            RTSP_session * rtsp_session)
{
    RTSP_Response *response = rtsp_response_new(req, RTSP_Ok);
    GString *rtp_info = g_string_new("");



    /* temporary string used for creating headers */
    GString *str = g_string_new("npt=");

    RTSP_Range *range = g_queue_peek_head(rtsp_session->play_requests);

    /* Create Range header */
    if (range->begin_time >= 0)
        g_string_append_printf(str, "%f", range->begin_time);

    g_string_append(str, "-");

    if (range->end_time > 0)
        g_string_append_printf(str, "%f", range->end_time);

    g_hash_table_insert(response->headers,
                        g_strdup(eris_hdr_range),
                        g_string_free(str, false));

    

    /* create Scale header*/
    if( g_hash_table_lookup(req->headers, "Scale") != NULL){
        float scale;
        if( rtsp_session->disableRateControl != 0 ) {
            scale = rtsp_session->scale_req;
        }else if(rtsp_session->scale_req >= 100 && rtsp_session->scale_req <= 132){
            if( rtsp_session->scale_req < 116 ) {
                scale = rtsp_session->scale + 100;
            }else{
                scale = 116;
            }

        }else{

            scale = rtsp_session->scale;
        }


        GString *scaleHeader = g_string_new("");
        g_string_append_printf(scaleHeader, "%f", scale);
        g_hash_table_insert(response->headers,
                            g_strdup(eris_hdr_scale),
                            g_string_free(scaleHeader, false));
    }

    /* Create RTP-Info header */
    g_slist_foreach(rtsp_session->rtp_sessions, rtp_session_send_play_reply, rtp_info);

    g_string_truncate(rtp_info, rtp_info->len-1);

    g_hash_table_insert(response->headers,
                        g_strdup(eris_hdr_rtp_info),
                        g_string_free(rtp_info, false));

    rtsp_response_send(response);
}

/**
 * @brief Parse the Range header and eventually add it to the session
 *
 * @param req The request to check and parse
 *
 *
 * @retval RTSP_Ok Parsing completed correctly.
 *
 * @retval RTSP_NotImplemented The Range: header specifies a format
 *                             that we don't implement (i.e.: clock,
 *                             smtpe or any new range type). Follows
 *                             specification of RFC 2326 Section
 *                             12.29.
 *
 * @retval RTSP_InvalidRange The provided range is not valid, which
 *                           means that the specified range goes
 *                           beyond the end of the resource.
 *
 * @retval RTSP_HeaderFieldNotValidforResource
 *                           A Range header with a range different
 *                           from 0- was present in a live
 *                           presentation session.
 *
 * RFC 2326 only mandates server to know NPT time, clock and smtpe
 * times are optional, and we currently support neither of them.
 *
 * Because both live555-based clients (VLC) and QuickTime always send
 * the Range: header even for live presentations, we have to accept
 * the range "0-" even if the mere presence of the Range header should
 * make the request invalid. Compare RFC 2326 Section 11.3.7.
 *
 * @see ragel_parse_range_header()
 */
static RTSP_ResponseCode parse_range_header(RTSP_Request *req)
{
    /* This is the default range to start from, if we need it. It sets
     * end_time and playback_time to negative values (that are invalid
     * and can be replaced), and the begin_time to zero (since if no
     * Range was specified, we want to start from the beginning).
     */
    static const RTSP_Range defaultrange = {
        .begin_time = 0,
        .end_time = -0.1,
        .playback_time = -0.1,
        .seek = FALSE
    };

    RTSP_session *session = req->client->session;
    const char *range_hdr = g_hash_table_lookup(req->headers, "Range");
    RTSP_Range *range;

    /* If we have no range header and there is no play request queued,
     * we interpret it as a request for the full resource, if there is one already,
     * we just start whatever range is currently selected.
     */
    if ( range_hdr == NULL &&
         (range = g_queue_peek_head(session->play_requests)) != NULL ) {
             
             
        range->seek = FALSE;
        range->playback_time = ev_now(session->srv->loop);


        fnc_log(FNC_LOG_VERBOSE,
            "PLAY [%f]: %f %f %f\n", ev_now(session->srv->loop),
            range->begin_time, range->end_time, range->playback_time);
            
        return RTSP_Ok;
    }



    /* Initialise the RTSP_Range structure by setting the three
     * values to the starting values. */
    range = g_slice_dup(RTSP_Range, &defaultrange);

    /* If there is any kind of parsing error, the range is considered
     * not implemented. It might not be entirely correct but until we
     * have better indications, it should be fine. */
    if (range_hdr) {
        
        fnc_log(FNC_LOG_VERBOSE, "Range header: %s\n", range_hdr);
        if ( !ragel_parse_range_header(range_hdr, range) ) {
            g_slice_free(RTSP_Range, range);
            /** @todo We should be differentiating between not-implemented and
             *        invalid ranges.
             */
            return RTSP_NotImplemented;
        }

        /* This is a very lax check on what range the client provided;
         * unfortunately both live555-based clients and QuickTime
         * always send a Range: header even if the SDP had no range
         * attribute, and there seems to be no way to tell them not to
         * (at least there is none with the current live555, not sure
         * about QuickTime).
         *
         * But just to be on the safe side, if we're streaming a
         * non-seekable resource (like live) and the resulting value
         * is not 0-inf, we want to respond with a "Header Field Not
         * Valid For Resource". If clients handled this correctly, the
         * mere presence of the Range header in this condition would
         * trigger that response.
         */
        if ( !session->resource->info->seekable &&
             range->begin_time != 0 &&
             range->end_time != -0.1 ) {
            g_slice_free(RTSP_Range, range);
            return RTSP_HeaderFieldNotValidforResource;
        }
        
        /* Jamken: because the client give a valid Range hader, 
         * we need to seek the resource before read */
        range->seek = TRUE; 

    }

    if(!session->resource->info->seekable){
        /* Jamken: For a non-seekable stream,  no end time is given, 
         * begin time is fix to 0
         */        
        range->begin_time = 0;
        range->end_time = -0.1;
    }else{

        if ( range->end_time > session->resource->info->duration){
            g_slice_free(RTSP_Range, range);
            return RTSP_InvalidRange;
        }else if(range->end_time < 0 ){
            /* Jamken: if no end_time but this is replay stream, 
             * give a default end time of the resource
             */
            range->end_time = session->resource->info->duration;
        }
        
    }
    



    if ( range->playback_time < 0 )
        range->playback_time = ev_now(session->srv->loop);

    if ( session->cur_state != RTSP_SERVER_PLAYING )
        rtsp_session_editlist_free(session);

    rtsp_session_editlist_append(session, range);

    fnc_log(FNC_LOG_VERBOSE,
            "PLAY [%f]: %f %f %f\n", ev_now(session->srv->loop),
            range->begin_time, range->end_time, range->playback_time);

    return RTSP_Ok;
}




/**
 * @brief Parse the scale header and eventually set it at the 
 *        session
 *
 * @param req The request to check and parse
 *
 *
 * @retval RTSP_Ok Parsing completed correctly.
 *
 * @retval RTSP_NotImplemented The Scale: header specifies a 
 *                             scale number that we don't
 *                             implement (we only support 0.25,
 *                             0.5, 1, 2, 4).
 *
 */
static RTSP_ResponseCode parse_scale_header(RTSP_Request *req)
{
    float scaleTable[20] = { 0.25, 0.5, 1, 2, 4, 8, 101, 102, 104, 108, 116, 255, 0,};
    /* 1xx for compatible for DB33T 629.2-2011 */

    RTSP_session *session = req->client->session;
    const char *scale_hdr = g_hash_table_lookup(req->headers, "Scale");
    float scaleValue;
    int i;


    /* If we have no scale header, just return OK.
     */
    if ( scale_hdr == NULL) {
        return RTSP_Ok;
    }
    if( scale_hdr != NULL ) {
        fnc_log(FNC_LOG_VERBOSE, "Scale header: %s\n", scale_hdr);
    }

   

    /* parse the scale value */
    while (*scale_hdr == ' ') ++scale_hdr;

    if (sscanf(scale_hdr, "%f", &scaleValue) == 1) {

        /* check the scale value is supported */
        i = 0;
        while ( scaleTable[i] != 0 ) {
            if( scaleTable[i] == scaleValue) {
                break;
            }
            i++;
        }
        if( scaleTable[i] == 0 ) {
            return RTSP_NotImplemented;
        }
        
        /* Jamken: for live stream, scale must be 1 */
        if(session->resource->info->media_source == MS_live && 
           scaleValue != 1.0){
              return RTSP_NotImplemented; 
        }

        session->scale = scaleValue;    
        session->scale_req = scaleValue;    

    } else {
        return RTSP_NotImplemented; // The header is malformed
    }

    if( session->scale >= 100 && session->scale <= 132 ) {
        if( session->scale >=  116) {
            session->scale = (float)session->srv->srvconf.max_rate;
        }else{
            session->scale = session->scale - 100;
        }
        session->download = 1;
    }else if(session->scale == 255){
        session->scale = (float)session->srv->srvconf.max_rate;
    }

    if( session->scale > (float)session->srv->srvconf.max_rate) {

        session->scale = (float)session->srv->srvconf.max_rate;
    }


    fnc_log(FNC_LOG_VERBOSE,
            "[rtsp] Configure scale %f to session\n", session->scale);

    return RTSP_Ok;
}


/**
 * @brief Parse the Frames header and eventually set it at the 
 *        session
 *
 * @param req The request to check and parse
 *
 *
 * @retval RTSP_Ok Parsing completed correctly.
 *
 * @retval RTSP_NotImplemented The Frames: header specifies 
 *                             whether send key frame only, can
 *                             only set to intra / all, for
 *                             other value return
 *                             RTSP_NotImplemented.
 *
 */
static RTSP_ResponseCode parse_frames_header(RTSP_Request *req)
{

    RTSP_session *session = req->client->session;
    const char *frames_hdr = g_hash_table_lookup(req->headers, "Frames");

    /* If we have no frames header, just return OK.
     */
    if ( frames_hdr == NULL  ) {
        return RTSP_Ok;
    }

    fnc_log(FNC_LOG_VERBOSE, "Frames header: %s\n", frames_hdr);

    if(strstr(frames_hdr,"intra") != NULL) {
        session->onlyKeyFrame = 1;
        
    }else if(strstr(frames_hdr,"all") != NULL){
        session->onlyKeyFrame = 0;
    }else{
        return RTSP_NotImplemented;

    }

    return RTSP_Ok;
}




/**
 * @brief Parse the Rate-Control header and eventually set scale
 *        at the session
 *
 * @param req The request to check and parse
 *
 *
 * @retval RTSP_Ok Parsing completed correctly.
 *
 * @retval RTSP_NotImplemented The Rate-Control: header 
 *                             specifies whether send frames at
 *                             max rate, can only set to yes /
 *                             no, for other value return
 *                             RTSP_NotImplemented.
 *
 */
static RTSP_ResponseCode parse_RateControl_header(RTSP_Request *req)
{

    RTSP_session *session = req->client->session;
    const char *RateControl_hdr = g_hash_table_lookup(req->headers, "Rate-Control");

    /* If we have no frames header, just return OK.
     */
    if ( RateControl_hdr == NULL  ) {
        return RTSP_Ok;
    }

    fnc_log(FNC_LOG_VERBOSE, "Rate-Control header: %s\n", RateControl_hdr);

    if(strstr(RateControl_hdr,"no") != NULL) {
        session->scale = (float) session->srv->srvconf.max_rate;
        session->disableRateControl = 1;
        
    }else if(strstr(RateControl_hdr,"yes") != NULL){
        session->disableRateControl = 0;
    }else{
        return RTSP_NotImplemented;

    }

    return RTSP_Ok;
}


/**
 * @brief Parse the Download header and eventually set it at the
 *        session
 *
 * @param req The request to check and parse
 *
 *
 * @retval RTSP_Ok Parsing completed correctly.
 *
 * @retval RTSP_NotImplemented The Download: header 
 *                             specifies whether this session is
 *                             used for download or not, can
 *                             only set to yes / no, default is
 *                             no. for other value return
 *                             RTSP_NotImplemented.
 *
 */
static RTSP_ResponseCode parse_Download_header(RTSP_Request *req)
{

    RTSP_session *session = req->client->session;
    const char *Download_hdr = g_hash_table_lookup(req->headers, "Download");

    /* If we have no frames header, just return OK.
     */
    if ( Download_hdr == NULL  ) {
        return RTSP_Ok;
    }

    fnc_log(FNC_LOG_VERBOSE, "Download header: %s\n", Download_hdr);

    if(strstr(Download_hdr,"no") != NULL) {
        session->download = 0;
        
    }else if(strstr(Download_hdr,"yes") != NULL){
        session->download = 1;
    }else{
        return RTSP_NotImplemented;

    }

    return RTSP_Ok;
}





/**
 * RTSP PLAY method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 */
void RTSP_play(RTSP_Client * rtsp, RTSP_Request *req)
{
    RTSP_session *rtsp_sess = rtsp->session;
    RTSP_ResponseCode error;
    const char *user_agent = NULL;

    if ( !rtsp_check_invalid_state(req, RTSP_SERVER_INIT) )
        return;

    if ( !rtsp_request_check_url(req) )
        return;

    if ( rtsp_sess->session_id == 0 ) {
        error = RTSP_BadRequest;
        goto error_management;
    }

    /* This is a workaround for broken VLC (up to 0.9.9 on *
     * 2008-04-22): instead of using a series of PLAY/PAUSE/PLAY for
     * seeking, it only sends PLAY requests. As per RFC 2326 Section
     * 10.5 this behaviour would suggest creating an edit list,
     * instead of seeking.
    */
    if ( rtsp_sess->cur_state == RTSP_SERVER_PLAYING ) {
        fnc_log(FNC_LOG_WARN, "Working around broken seek of %s", user_agent);
        rtsp_do_pause(rtsp);
    }

    if ( (error = parse_range_header(req)) != RTSP_Ok )
        goto error_management;


    if ( (error = parse_scale_header(req)) != RTSP_Ok )
        goto error_management;

    if ( (error = parse_Download_header(req)) != RTSP_Ok )
        goto error_management;

    if ( (error = parse_RateControl_header(req)) != RTSP_Ok )
        goto error_management;

    if( g_hash_table_lookup(req->headers, "Frames") != NULL ) {

        if ( (error = parse_frames_header(req)) != RTSP_Ok )
            goto error_management;

    }else{
    
    
        if((user_agent = g_hash_table_lookup(req->headers, eris_hdr_user_agent)) &&
           (strncmp(user_agent, "VLC media player", strlen("VLC media player")) == 0 ||
            strncmp(user_agent, "LibVLC", strlen("LibVLC")) == 0)){
    
            if( rtsp_sess->scale > 1 ) {
                fnc_log(FNC_LOG_WARN, "send key frame only for %s scale > 1", user_agent);  
                rtsp_sess->onlyKeyFrame = 1;      
            }else{
                rtsp_sess->onlyKeyFrame = 0;   
            }
        }
    }
    /* control audio skip */
    if((user_agent = g_hash_table_lookup(req->headers, eris_hdr_user_agent)) &&
       (strncmp(user_agent, "VLC media player", strlen("VLC media player")) == 0 ||
        strncmp(user_agent, "LibVLC", strlen("LibVLC")) == 0)){
    
        if( rtsp_sess->scale > 1 ) {
            fnc_log(FNC_LOG_WARN, "skip audio sample for %s scale > 1", user_agent);  
            rtsp_sess->simpleAudioSkip = 1;      
        }else{
            rtsp_sess->simpleAudioSkip = 0;   
        }
    }


    if ( rtsp_sess->cur_state != RTSP_SERVER_PLAYING &&
         (error = do_play(rtsp_sess)) != RTSP_Ok )
        goto error_management;

    send_play_reply(req, rtsp_sess);

    ev_timer_again (rtsp->srv->loop, &rtsp->ev_timeout);

    return;

error_management:
    rtsp_quick_response(req, error);
    return;
}
