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


client_port_pair *current_client = NULL;

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
    
    if(current_client != NULL){
        free_child_port(current_client);
        current_client = NULL;
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


