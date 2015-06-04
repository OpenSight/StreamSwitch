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
 * @brief Contains OPTIONS method and reply handlers
 */

#include <liberis/headers.h>

#include "rtsp.h"

/**
 * RTSP OPTIONS method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 * @return ERR_NOERROR
 */
void RTSP_options(RTSP_Client * rtsp, RTSP_Request *req)
{
    RTSP_Response *response = rtsp_response_new(req, RTSP_Ok);

    g_hash_table_insert(response->headers, 
                        g_strdup(eris_hdr_public),
                        g_strdup("OPTIONS,DESCRIBE,SETUP,PLAY,PAUSE,TEARDOWN"));

    rtsp_response_send(response);
}
