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
#include "config.h"
#include "fnc_log.h"

GList *child_head=NULL;
int reach_play=0;




static void destroy_client_item(gpointer elem,
                              ATTR_UNUSED gpointer unused) {
    if(elem != NULL){
		free_child_port((client_port_pair*)elem);
    }
}

void add_client_list(client_port_pair* client)
{
	child_head = g_list_append(child_head,client);
}

void reduce_client_list(client_port_pair* client)
{
	child_head = g_list_remove(child_head,client);
}
void init_client_list()
{
    child_head = NULL;
}
void free_client_list()
{
    if(child_head != NULL){
	  g_list_foreach(child_head, destroy_client_item, NULL);
        g_list_free(child_head);
        child_head = NULL;
        
    }
}

client_port_pair* new_child_port(struct feng *srv, 
                                 char * host, unsigned short port)
{

	client_port_pair *client_port = g_new0(client_port_pair,1);
	client_port->srv = srv;
    if(host != NULL){
        client_port->host = g_strdup(host);
    }
    client_port->port = port;
	return client_port;
}

void free_child_port(client_port_pair *client)
{

	if(client)
	{
        if(client->host != NULL){
            g_free(client->host);
            client->host = NULL;
        }
		g_free(client);
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


