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

/**
 * @file
 * Port handling utility functions
 */

#include "feng.h"
#include "feng_utils.h"
#include "rtp.h"

static int start_port; //!< initial rtp port
static int *port_pool; //!< list of allocated ports

#ifdef TRISOS
GList *child_head=NULL;
int reach_play=0;
#endif
#ifdef CLEANUP_DESTRUCTOR
/**
 * @brief Cleanup funciton for the @ref port_pool array
 *
 * This function is used to free the @ref port_pool array that was
 * allocated in @ref RTP_port_pool_init; note that this function is
 * called automatically as a destructor when the compiler supports it,
 * and debug is not disabled.
 *
 * This function is unnecessary on production code, since the memory
 * would be freed only at the end of execution, when the resources
 * would be freed anyway.
 *
 * @note Part of the cleanup destructors code, not compiled in
 *       production use.
 */
static void CLEANUP_DESTRUCTOR rtp_port_cleanup()
{
    g_free(port_pool);
}
#endif

/**
 * Initializes the pool of ports and the initial port from the given one
 * @param srv The server instance in use (for ONE_FORK_MAX_CONNECTION)
 * @param port The first port from which to start allocating connections
 */
void RTP_port_pool_init(feng *srv, int port)
{
    int i;
    start_port = port;
#ifndef TRISOS
    port_pool = g_new(int, ONE_FORK_MAX_CONNECTION);
    for (i = 0; i < ONE_FORK_MAX_CONNECTION; ++i) {
#else
    port_pool = g_new(int, 2*ONE_FORK_MAX_CONNECTION);
    for (i = 0; i < 2*ONE_FORK_MAX_CONNECTION; ++i) {
#endif
        port_pool[i] = i + start_port;
    }


}

/**
 * Gives a group of available ports for a new connection
 *
 * @param srv The server instance in use (for ONE_FORK_MAX_CONNECTION)
 * @param pair where to set the group of available ports
 *
 * @retval ERR_NOERROR No error
 * @retval ERR_GENERIC No port available
 */
int RTP_get_port_pair(feng *srv, port_pair * pair)
{
    int i;

#ifndef TRISOS
    for (i = 0; i < ONE_FORK_MAX_CONNECTION; ++i) {
#else
    for (i = 0; i < 2*ONE_FORK_MAX_CONNECTION; ++i) {
#endif
        if (port_pool[i] != 0) {
            pair->RTP =
                (port_pool[i] - start_port) * 2 + start_port;
            pair->RTCP = pair->RTP + 1;
            port_pool[i] = 0;
            return ERR_NOERROR;
        }
    }

    return ERR_GENERIC;
}
#ifdef TRISOS

 static void destroy_client_item(gpointer elem,
                              ATTR_UNUSED gpointer unused) {
		free_child_port((client_port_pair*)elem);
}

void add_client_list(client_port_pair* client)
{
	child_head = g_list_append(child_head,client);
}

void reduce_client_list(client_port_pair* client)
{
	child_head = g_list_remove(child_head,client);
}
void free_client_list()
{
	  g_list_foreach(child_head, destroy_client_item, NULL);
        g_list_free(child_head);
        child_head = NULL;
}

client_port_pair* new_child_port(feng *srv)
{
	int i=0;
	client_port_pair *client_port = g_new0(client_port_pair,1);
	client_port->srv = srv;
	for (i = 0; i < 2; i++) 
	{
		if(!(client_port->RTP_port[i] =  get_child_port(srv)))
			goto error;
	}
	return client_port;
error:	
	free_child_port(client_port);
	client_port = NULL;
	return NULL;	
}

void free_child_port(client_port_pair *client)
{
	int i=0;
	if(client)
	{
		for(i=0;i<2;i++)
		{
            if(client->RTP_port[i] != 0) {
                release_child_port(client->srv,client->RTP_port[i]);
            }
		}
		g_free(client);
	}
}
int  get_child_port(feng *srv)
{
	int ret;
	port_pair pair = {0, 0};
	if(RTP_get_port_pair(srv, &pair) != ERR_NOERROR)
	goto error;

	ret = pair.RTP;

	return ret;
error:
	return 0;
}
void release_child_port(feng *srv,int RTP_port)
{
	port_pair pair = {0, 0};
	if(RTP_port)
	{

		pair.RTP = RTP_port;
        pair.RTCP = RTP_port + 1;

		RTP_release_port_pair(srv, &pair);

	}
}

client_port_pair *get_client_list_item(pid_t pid)
{
	GList *item=NULL;
	client_port_pair *client = NULL;
	item = child_head;
	while(item)
	{
		client = (client_port_pair *)item->data;
		if(client->pid == pid)
		{
			break;
		}
		item = g_list_next(item);
	}
	return client;
}

#endif
/**
 * Sets a group of ports as available for a new connection
 *
 * @param srv The server instance in use (for ONE_FORK_MAX_CONNECTION)
 * @param pair the group of ports to release
 *
 * @retval ERR_NOERROR No error
 * @retval ERR_GENERIC Ports not allocated
 */
int RTP_release_port_pair(feng *srv, port_pair * pair)
{
    int i;
#ifndef TRISOS
    for (i = 0; i < ONE_FORK_MAX_CONNECTION; ++i) {
#else
    for (i = 0; i < 2*ONE_FORK_MAX_CONNECTION; ++i) {
#endif
        if (port_pool[i] == 0) {
            port_pool[i] =
                (pair->RTP - start_port) / 2 + start_port;
            return ERR_NOERROR;
        }
    }
    return ERR_GENERIC;
}
