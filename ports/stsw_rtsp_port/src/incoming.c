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

#include <stdbool.h>
#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <string.h>


#ifndef __WIN32__   
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "feng.h"
#include "fnc_log.h"
#include "incoming.h"
#include "network/rtsp.h"


/**
 * @brief libev listeners for incoming connections on opened ports
 *
 * This is an array of ev_io objects allocated with the g_new0()
 * function (which this need to be freed with g_free()).
 *
 * The indexes are the same as for @ref feng::config_storage.
 *
 * @note Part of the cleanup destructors code, not compiled in
 *       production use.
 */
static ev_io *listeners;

/**
 * @brief List of listening sockets for the process
 *
 * This array is created in @ref feng_bind_ports and used in @ref
 * feng_bind_port if the cleanup destructors are enabled, and will be
 * used by @ref feng_ports_cleanup() to close the sockets and free
 * their memory.
 *
 * @note Part of the cleanup destructors code, not compiled in
 *       production use.
 */
static GPtrArray *listening_sockets;

/**
 * @brief Close sockets in the listening_sockets array
 *
 * @param element The Sock object to close
 * @param user_data Unused
 *
 * This function is used to close the opened software during cleanup.
 *
 * @note Part of the cleanup destructors code, not compiled in
 *       production use.
 */
static void feng_bound_socket_close(gpointer element,
                                    gpointer user_data)
{
    Sock_close((Sock*)element);
}

static void listeners_stop(feng *srv,ev_io* listener)
{
	ev_io_stop(srv->loop,listener);
}



/**
 * @brief Cleanup function for the @ref listeners array
 *
 * This function is used to free the @ref listeners array that was
 * allocated in @ref feng_bind_ports; note that this function is
 * called automatically as a destructur when the compiler supports it,
 * and debug is not disabled.
 *
 * This function is unnecessary on production code, since the memory
 * would be freed only at the end of execution, when the resources
 * would be freed anyway.
 *
 * @note Part of the cleanup destructors code, not compiled in
 *       production use.
 */
void  feng_ports_cleanup(feng *srv)
{
    //int i=0;
    g_ptr_array_foreach(listening_sockets, feng_bound_socket_close, NULL);
    g_ptr_array_free(listening_sockets, true);
    listeners_stop(srv,listeners);
    g_free(listeners);

}






static int socket_keep_alive( int fd )
{
   int optval;
   socklen_t optlen = sizeof(optval);

   int keepIdle = 60; // Èç¸ÃÁ¬½ÓÔÚ60ÃëÄÚÃ»ÓÐÈÎºÎÊý¾ÝÍùÀ´,Ôò½øÐÐÌ½²â 
   int keepInterval = 5; // Ì½²âÊ±·¢°üµÄÊ±¼ä¼ä¸ôÎª5 Ãë
   int keepCount = 3; // Ì½²â³¢ÊÔµÄ´ÎÊý.Èç¹ûµÚ1´ÎÌ½²â°ü¾ÍÊÕµ½ÏìÓ¦ÁË,Ôòºó2´ÎµÄ²»ÔÙ·¢.


#ifndef __WIN32__   
   /* Check the status for the keepalive option */
   if(getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
        return -1;
   }
   //printf("SO_KEEPALIVE is %s\n", (optval ? "ON" : "OFF"));

   /* Set the option active */
   optval = 1;
   optlen = sizeof(optval);
   if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
       return -1;
   }
   //printf("SO_KEEPALIVE set on socket\n");

   if(setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle)) < 0) {
       return -1;
   }
   if(setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) < 0) {
       return -1;
   }
   if(setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount))< 0) {
       return -1;
   }
#endif

   return 0;



}


/**
 * Bind to the defined listening port
 *
 * @param srv The server instance to bind ports for
 * @param host The hostname to bind ports on
 * @param port The port to bind
 * @param s The specific configuration from @ref feng::config_storage
 * @param listener The listener pointer from @ref listeners
 *
 * @retval true Binding complete
 * @retval false Error during binding
 */
static gboolean feng_bind_port(feng *srv, const char *host, const char *port,
                               specific_config *s, ev_io *listener)
{
    gboolean is_sctp = !!s->is_sctp;   
    Sock *sock;

    if (is_sctp)
        sock = Sock_bind(host, port, NULL, SCTP, NULL);
    else
        sock = Sock_bind(host, port, NULL, TCP, NULL);
    sock = Sock_bind(host, port, NULL, TCP, NULL);
    if(!sock) {
        fnc_log(FNC_LOG_ERR,"Sock_bind() error for port %s.", port);
        fprintf(stderr,
                "[fatal] Sock_bind() error in main() for port %s.\n",
                port);
        return false;
    }

    if(socket_keep_alive(Sock_fd(sock))) {
        fnc_log(FNC_LOG_ERR,"socket_keep_alive() error: %s", strerror(errno));
        return false;
    }

    g_ptr_array_add(listening_sockets, sock);

    fnc_log(FNC_LOG_INFO, "Listening to port %s (%s) on %s",
            port,
            (is_sctp? "SCTP" : "TCP"),
            ((host == NULL)? "all interfaces" : host));

    if(Sock_listen(sock, SOMAXCONN)) {
        fnc_log(FNC_LOG_ERR, "Sock_listen() error for TCP socket.");
        fprintf(stderr, "[fatal] Sock_listen() error for TCP socket.\n");
        return false;
    }    sock->data = srv;
    listener->data = sock;
    ev_io_init(listener,
               rtsp_client_incoming_cb,
               Sock_fd(sock), EV_READ);
    ev_io_start(srv->loop, listener);

    return true;
}

gboolean feng_bind_ports(feng *srv)
{
    //size_t i;
    char *host = srv->srvconf.bindhost->ptr;
    char port[6] = { 0, };

    snprintf(port, sizeof(port), "%d", srv->srvconf.port);

    /* This is either static or local, we don't care */
    listeners = g_new0(ev_io, 1);

    listening_sockets = g_ptr_array_sized_new(1);


    if (!feng_bind_port(srv, host, port,
                        &(srv->config_storage),
                        &listeners[0])){
        return false;
    }


    return true;
}
