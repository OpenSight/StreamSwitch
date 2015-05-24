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

#include "feng.h"
#include "network/rtsp.h"
#include "network/rtp.h"
#include "fnc_log.h"
#include "media/demuxer.h"

#ifdef TRISOS
#include <sys/wait.h>
#include <sys/signal.h>

#endif
#include <sys/time.h>
#include <stdbool.h>
#include <ev.h>

#define LIVE_STREAM_BYE_TIMEOUT 6
#define STREAM_TIMEOUT 12 /* This one must be big enough to permit to VLC to switch to another
                             transmission protocol and must be a multiple of LIVE_STREAM_BYE_TIMEOUT */

/**
 * @brief Handle client disconnection and free resources
 *
 * @param loop The event loop where the event was issued
 * @param w The async event object
 * @param revents Unused
 *
 * This event is triggered when a client disconnects or is forcefully
 * disconnected. It stops the other events from running, and frees all
 * the remaining resources for the client itself.
 */
static void client_ev_disconnect_handler(struct ev_loop *loop,
                                         ev_async *w,
                                         int revents)
{
    RTSP_Client *rtsp = (RTSP_Client*)w->data;
    GString *outbuf = NULL;
    feng *srv = rtsp->srv;

    ev_io_stop(srv->loop, &rtsp->ev_io_read);
    ev_io_stop(srv->loop, &rtsp->ev_io_write);
    ev_async_stop(srv->loop, &rtsp->ev_sig_disconnect);
    ev_timer_stop(srv->loop, &rtsp->ev_timeout);

    Sock_close(rtsp->sock);
    srv->connection_count--;

    rtsp_session_free(rtsp->session);

    interleaved_free_list(rtsp);

    /* Remove the output queue */
    while( (outbuf = g_queue_pop_tail(rtsp->out_queue)) )
        g_string_free(outbuf, TRUE);

    g_queue_free(rtsp->out_queue);

    g_byte_array_free(rtsp->input, true);

    g_slice_free(RTSP_Client, rtsp);

    fnc_log(FNC_LOG_INFO, "[client] Client removed");

	sleep(1);
	exit(0);

}

static void check_if_any_rtp_session_timedout(gpointer element,
                                              gpointer user_data)
{
    RTP_session *session = (RTP_session *)element;
    time_t *last_packet_send_time = (time_t *)user_data;
    time_t now = time(NULL);
    
    /* Jamken: get the last packet send time in all the session*/
    if( last_packet_send_time != NULL &&
        (session->last_packet_send_time > *last_packet_send_time){
        *last_packet_send_time = session->last_packet_send_time;
    }
    
#if 0
    /* Check if we didn't send any data for more then STREAM_BYE_TIMEOUT seconds
     * this will happen if we are not receiving any more from live producer or
     * if the stored stream ended.
     */
    if ((session->track->properties.media_source == MS_live) &&
        (now - session->last_packet_send_time) >= LIVE_STREAM_BYE_TIMEOUT) {
        fnc_log(FNC_LOG_INFO, "[client] Soft stream timeout");
        rtcp_send_sr(session, BYE);
    }

    /* If we were not able to serve any packet and the client ignored our BYE
     * kick it by closing everything
     */
    if (session->isBye != 0 && 
        (now - session->last_packet_send_time) >= STREAM_TIMEOUT) {
        fnc_log(FNC_LOG_INFO, "[client] Stream Timeout, client kicked off!");
        ev_async_send(session->srv->loop, &session->client->ev_sig_disconnect);
    }else{
#endif        
        /* send RTCP SDE */
        rtcp_send_sr(session, SDES);

        /* If we do not read RTCP	report in 12 seconds .we will deal it as the client lost
        connection,and end the child process.
        */

        if(session->srv->srvconf.rtcp_heartbeat != 0 &&
           session->last_rtcp_read_time != 0 &&
           (now - session->last_rtcp_read_time)>=60)
        {
            fnc_log(FNC_LOG_INFO, "[client] Client Lost Connection\n");
            ev_async_send(session->srv->loop, &session->client->ev_sig_disconnect);
        }
#if 0
    }
#endif
}

static void client_ev_timeout(struct ev_loop *loop, ev_timer *w,
                               int revents)
{
    RTSP_Client *rtsp = w->data;
    time_t last_packet_send_time = 0;
    time_t now; = time(NULL);
    if(rtsp->session->rtp_sessions){
        g_slist_foreach(rtsp->session->rtp_sessions,
                        check_if_any_rtp_session_timedout, 
                        &last_packet_send_time);
                        
    }
    
    //check stream timeout
    now = time(NULL);
    if((now - last_packet_send_time) >= STREAM_TIMEOUT){
        fnc_log(FNC_LOG_ERR, "Stream Timeout, client kicked off!");
        ev_async_send(rtsp->srv->loop, &rtsp->ev_sig_disconnect);        
    }
    
    ev_timer_again (loop, w);
}

static void child_terminate_cb (struct ev_loop *loop, ev_child *w, int revents)
{
     //ev_child_stop (EV_A_ w);
    printf ("process %d exited with status %x\n", w->rpid, w->rstatus);
    
    pid_t pid = w->rpid;
    client=get_client_list_item(pid);
    if(client)
    {
        reduce_client_list(client);

        srv->connection_count--;

        free_child_port(client);

    }    
    
}

ev_child cw;

void feng_start_child_watcher(feng *srv)
{
    ev_child_init (&cw, child_terminate_cb, 0, 0);
    ev_child_start (srv->loop, &cw);    
}

void feng_stop_child_watcher(feng *srv)
{
    ev_child_sop (srv->loop, &cw);    
}

#if 0

static void timer_watcher_cb(struct ev_loop *loop, ev_timer *w,
                               int revents)
{
#ifndef __WIN32__    
	pid_t pid;
	feng *srv=w->data;
	client_port_pair *client=NULL;
	while(1)
	{
		pid = waitpid(-1,NULL, 0);
		if(pid>0)
		{
			client=get_client_list_item(pid);
			if(client)
			{
                reduce_client_list(client);

                srv->connection_count--;

                free_child_port(client);

			}

		}
		else
		break;
	}
#endif    
	ev_timer_again(loop, w);
}
void loop_timer_init(feng* srv)
{
	ev_timer *timer;
	timer = &srv->loop_timer;
	timer->data = srv;
	ev_init(timer, timer_watcher_cb);
	timer->repeat = 5.;

    ev_timer_again(srv->loop, &srv->loop_timer);
}

void loop_timer_uninit(feng* srv)
{
    ev_timer_stop(srv->loop, &srv->loop_timer);
}

#endif

/**
 * @brief Handle an incoming RTSP connection
 *
 * @param loop The event loop where the incoming connection was triggered
 * @param w The watcher that triggered the incoming connection
 * @param revents Unused
 *
 * This function takes care of all the handling of an incoming RTSP
 * client connection:
 *
 * @li accept the new socket;
 *
 * @li checks that there is space for new connections for the current
 *     fork;
 *
 * @li creates and sets up the @ref RTSP_Client object.
 *
 * The newly created instance is deleted by @ref
 * client_ev_disconnect_handler.
 *
 * @internal This function should be used as callback for an ev_io
 *           listener.
 */
void rtsp_client_incoming_cb(struct ev_loop *loop, ev_io *w,
                             int revents)
{
    Sock *sock = w->data;
    feng *srv = sock->data;
    Sock *client_sock = NULL;
    ev_io *io;
    ev_async *async;
    ev_timer *timer;
    RTSP_Client *rtsp;


    client_port_pair *clients=NULL;
    pid_t pid;


    if ( (client_sock = Sock_accept(sock, NULL)) == NULL )
        return;

    if (srv->connection_count >= ONE_FORK_MAX_CONNECTION) {
        Sock_close(client_sock);
        return;
    }

    if(!( clients = new_child_port(srv))){
        Sock_close(client_sock);
        return;
    }
    pid = fork();
    if(pid==0){
        
        /*clean the context of parent*/
        free_child_port(clients);


        feng_ports_cleanup(srv);
        
        feng_stop_child_watcher(srv);
        

        //void fnc_log_change_child();
        if(srv->srvconf.log_type == FNC_LOG_FILE){
            //child process can't write to parent log file
            fnc_log_uninit();            
        }
        


        rtsp = g_slice_new0(RTSP_Client);
        rtsp->sock = client_sock;
        rtsp->input = g_byte_array_new();
        rtsp->out_queue = g_queue_new();
        rtsp->srv = srv;

        srv->connection_count++;
        client_sock->data = srv;
        
        /*install read handler*/
        io = &rtsp->ev_io_read;
        io->data = rtsp;
        ev_io_init(io, rtsp_read_cb, Sock_fd(client_sock), EV_READ);
        ev_io_start(srv->loop, io);
        
        /* configure the write handler*/
        /* to be started/stopped when necessary */
        io = &rtsp->ev_io_write;
        io->data = rtsp;
        ev_io_init(io, rtsp_write_cb, Sock_fd(client_sock), EV_WRITE);
        fnc_log(FNC_LOG_INFO, "Incoming RTSP connection accepted on socket: %d\n",
            Sock_fd(client_sock));
        
        /* install async event handler for destroy */
        async = &rtsp->ev_sig_disconnect;
        async->data = rtsp;
        ev_async_init(async, client_ev_disconnect_handler);
        ev_async_start(srv->loop, async);
        
        /* configure a check timer, 
         * After play, this timer would be started */
        timer = &rtsp->ev_timeout;
        timer->data = rtsp;
        ev_init(timer, client_ev_timeout);
        timer->repeat = STREAM_TIMEOUT;



    }else if(pid > 0){
        srv->connection_count++;
        Sock_close(client_sock);
        clients->pid = pid;
        add_client_list(clients);
        fnc_log(FNC_LOG_INFO, "Connection reached: %d\n", srv->connection_count);
        
        return;
    }else{
        Sock_close(client_sock);
        free_child_port(clients);
        fnc_log(FNC_LOG_ERR, "fork faild\n");
        return;
    }

}
