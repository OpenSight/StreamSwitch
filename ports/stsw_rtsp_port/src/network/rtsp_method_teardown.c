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
 * @brief Contains TEARDOWN method and reply handlers
 */

#include "rtsp.h"

/**
 * RTSP TEARDOWN method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 * @todo trigger the release of rtp resources here
 */
void RTSP_teardown(RTSP_Client *rtsp,
                   RTSP_Request *req)
{
    if ( !rtsp_request_check_url(req) )
        return;

//    ev_async_send(rtsp->srv->loop, rtsp->ev_sig_disconnect);

    rtsp_quick_response(req, RTSP_Ok);
}
