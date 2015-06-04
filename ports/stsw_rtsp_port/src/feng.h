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

/**
 * @file feng.h
 * Server instance structure
 */

#ifndef FN_SERVER_H
#define FN_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <netembryo/wsocket.h>
#include <ev.h>
#include <unistd.h>
#ifndef __WIN32__  
#include <pwd.h>
#endif

#include "conf/array.h"
#include "conf/conf.h"





typedef struct feng {




    specific_config config_storage;

    server_config srvconf;

    struct ev_loop *loop;       //!< event loop


    ev_timer loop_timer;

    /**
     * @brief Number of active connections
     *
     * Once it reaches the maximum the server is supposed to redirect
     * to a twin if available
     */
    size_t connection_count;

    GMutex *lock;        //!< lock to access live_mq
    
    pid_t pid;
} feng;

typedef feng server;

#define MAX_PROCESS    1    /*! number of fork */
#define MAX_CONNECTION    srv->srvconf.max_conns   /*! rtsp connection */
#define ONE_FORK_MAX_CONNECTION ((int)(MAX_CONNECTION/MAX_PROCESS)) /*! rtsp connection for one fork */


#ifdef __cplusplus
}
#endif //

#endif // FN_SERVER_H
