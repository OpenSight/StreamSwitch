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

#include <strings.h>
#include <errno.h>
#include <netembryo/wsocket.h>

#include "feng.h"
#include "rtsp.h"
#include "fnc_log.h"
#include "feng_utils.h"


/**
 * @brief Read data out of an RTSP client socket
 *
 * @param rtsp The client to read data from
 * @param buffer Pointer to the memory area to read
 * @param size The size of the memory area to read
 *
 * @return The amount of data read from the socket
 * @return -1 Error during read.
 *
 * @todo The size parameter and the return value should be size_t
 *       instead of int.
 */
static int rtsp_sock_read(RTSP_Client *rtsp, char *buffer, int size)
{
    Sock *sock = rtsp->sock;
    int n;

    n = Sock_read(sock, buffer, size, NULL, 0);

    return n;
}

void rtsp_read_cb(struct ev_loop *loop, ev_io *w,
                  int revents)
{
    char buffer[RTSP_BUFFERSIZE + 1] = { 0, };    /* +1 to control the final '\0' */
    int read_size;
    RTSP_Client *rtsp = w->data;

    if ( (read_size = rtsp_sock_read(rtsp,
                                     buffer,
                                     sizeof(buffer) - 1)
          ) <= 0 )
        goto client_close;


    if (rtsp->input->len + read_size > RTSP_BUFFERSIZE) {
        fnc_log(FNC_LOG_DEBUG,
                "RTSP buffer overflow (input RTSP message is most likely invalid).\n");
        goto server_close;
    }

    g_byte_array_append(rtsp->input, (guint8*)buffer, read_size);
    if (RTSP_handler(rtsp) == ERR_GENERIC) {
        fnc_log(FNC_LOG_ERR, "Invalid input message.\n");
        goto server_close;
    }

    return;

 client_close:
    fnc_log(FNC_LOG_INFO, "RTSP connection closed by client.");
    goto disconnect;

 server_close:
    fnc_log(FNC_LOG_INFO, "RTSP connection closed by server.");
    goto disconnect;

 disconnect:
    ev_async_send(loop, &rtsp->ev_sig_disconnect);
}


void rtsp_write_cb( struct ev_loop *loop, ev_io *w,
                    int revents)
{
    RTSP_Client *rtsp = w->data;

    GByteArray *outpkt = NULL;

    while (1) {

        outpkt = (GByteArray *)g_queue_pop_tail(rtsp->out_queue);
    
        if (outpkt == NULL) {
            ev_io_stop(rtsp->srv->loop, &rtsp->ev_io_write);
            return;
        }
    
        if ( Sock_write(rtsp->sock, outpkt->data, outpkt->len,
                        NULL, 0) < outpkt->len) {
    
            switch (errno) {
                case EACCES:
                    fnc_log(FNC_LOG_ERR, "EACCES error");
                    break;
                case EAGAIN:
                    fnc_log(FNC_LOG_ERR, "EAGAIN error");
                    break;
                case EBADF:
                    fnc_log(FNC_LOG_ERR, "EBADF error");
                    break;
#ifndef __WIN32__                    
                case ECONNRESET:
                    fnc_log(FNC_LOG_ERR, "ECONNRESET error");
                    break;
                case EDESTADDRREQ:
                    fnc_log(FNC_LOG_ERR, "EDESTADDRREQ error");
                    break;
                case EISCONN:
                    fnc_log(FNC_LOG_ERR, "EISCONN error");
                    break;
                case EMSGSIZE:
                    fnc_log(FNC_LOG_ERR, "EMSGSIZE error");
                    break;
                case ENOBUFS:
                    fnc_log(FNC_LOG_ERR, "ENOBUFS error");
                    break;   
                case ENOTCONN:
                    fnc_log(FNC_LOG_ERR, "ENOTCONN error");
                    break;
                case ENOTSOCK:
                    fnc_log(FNC_LOG_ERR, "ENOTSOCK error");
                    break;
                case EOPNOTSUPP:
                    fnc_log(FNC_LOG_ERR, "EOPNOTSUPP error");
                    break;
#endif                 
                case EFAULT:
                    fnc_log(FNC_LOG_ERR, "EFAULT error");
                    break;
                case EINTR:
                    fnc_log(FNC_LOG_ERR, "EINTR error");
                    break;
                case EINVAL:
                    fnc_log(FNC_LOG_ERR, "EINVAL error");
                    break;

                case ENOMEM:
                    fnc_log(FNC_LOG_ERR, "ENOMEM error");
                    break;

                case EPIPE:
                    fnc_log(FNC_LOG_ERR, "EPIPE error");
                    break;
                default:
                    break;
            }
            fnc_log(FNC_LOG_ERR, "Sock_write() error in rtsp_send()");

            exit(-1);
        }
    
        g_byte_array_free(outpkt, TRUE);
    
    }
}



