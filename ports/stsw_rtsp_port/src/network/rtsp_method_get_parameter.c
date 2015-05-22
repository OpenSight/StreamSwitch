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

/** @file
 * @brief Contains PAUSE method and reply handlers
 */

#include "feng.h"
#include "rtsp.h"
#include "rtp.h"
#include <string.h>
#include <stdbool.h>

#include <liberis/headers.h>

#define SDP_EL "\r\n"


static GString *parameter_desc(RTSP_Client * rtsp, RTSP_Request *req)
{
    GString *descr = NULL;
    char const* params = req->body;
    char param[256];

    if( params == NULL ) {
        return NULL;
    }

    descr = g_string_new("");
    
    while (sscanf(params, "%[^\r\n]", param) == 1) {

        if( !strcmp(param, "session_ended") &&
            rtsp->session != NULL ) {
            g_string_append_printf(descr, "session_ended: %s"SDP_EL,
                               rtsp->session->started==0?"Yes":"No");
        }

        params += strlen(param);
        while (params[0] == '\r' || params[0] == '\n') ++params; // skip over all leading '\r\n' chars
        if (params[0] == '\0') break;
    }

    if( descr->len == 0 ) {
        g_string_free(descr, true);
        descr = NULL;
    }

    return descr;

}
/**
 * Sends the reply for the get_parameter method
 * @param req The client request for the method
 * @param descr the description string to send
 */
static void send_get_parameter_reply(RTSP_Request *req, GString *descr)
{
    RTSP_Response *response = rtsp_response_new(req, RTSP_Ok);

    /* bluntly put it there */
    response->body = descr;

    /* When we're going to have more than one option, add alternatives here */
    g_hash_table_insert(response->headers,
                        g_strdup(eris_hdr_content_type),
                        g_strdup("text/parameters"));

    rtsp_response_send(response);
}

/**
 * RTSP GET_PARAMETER method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 */
void RTSP_get_parameter(RTSP_Client * rtsp, RTSP_Request *req)
{
    GString *descr;
    /* This is only valid in Playing state */
    if ( !rtsp_check_invalid_state(req, RTSP_SERVER_INIT))
        return;

    if ( !rtsp_request_check_url(req) )
        return;

    // Get Session Description
    descr = parameter_desc(rtsp, req);

    send_get_parameter_reply(req,descr);
}
