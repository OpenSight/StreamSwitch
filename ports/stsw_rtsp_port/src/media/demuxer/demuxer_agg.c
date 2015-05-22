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
#ifdef TRISOS

#define _GNU_SOURCE
#include <dirent.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#include "feng.h"
#include "feng_utils.h"
#include "fnc_log.h"

#include "media/demuxer_module.h"

#include <vfm.h>

#define FILE_PATHNAME_MAX_LEN VFM_MAX_PATH_SIZE

static const DemuxerInfo info = {
    "AGG Demuxer",
    "agg",
    "BPP Team",
    "",
    "db"
};
typedef struct edl_item_elems{
    Resource *r;
    double begin;	//from begin of list, in seconds
    double end;     //from begin of list, in seconds
    double file_start;  // the offset of begin point within the file 
    char filename[FILE_PATHNAME_MAX_LEN]; //inner path name
    int index;
}edl_item_elems;
typedef struct edl_priv_datas{
    GList *head;
    GList *active;
}edl_priv_datas;
typedef struct Filele {
    char filename[FILE_PATHNAME_MAX_LEN]; //inner path name

    struct timeval file_start_time;
    struct timeval file_end_time;
    struct timeval file_offset;
    struct timeval file_length;

}Filele;

static int get_after_flag_length(char flag, const char *str)
{
	char *dest;
	if(!str) return 0;
	dest = strrchr(str, flag);
	if(dest)
	{
		return strlen(dest);
	}
	else
	return 0;
}
static void get_file_path(char  *dest_path,const char *source_path)
{
	int str_len,temp;
	str_len = get_after_flag_length('/',source_path);
	if(str_len == 0)
	return;
	temp = strlen(source_path)-str_len+1;
	strncpy(dest_path,source_path,strlen(source_path)-str_len+1);
	dest_path[temp] = '\0';
}

static void get_inner_path(Resource * r, char  *inner_path,const char *full_path)
{
	struct feng *srv = r->srv;
	char * base_path = srv->config_storage[0].document_root->ptr;

	int base_len;

	/* get path */
	base_len = strlen(base_path);

	assert(strncmp(full_path, base_path, base_len) == 0 );
	assert(full_path[base_len] == '/' );

	strcpy(inner_path, full_path + base_len + 1);

}

static void destroy_list_data(gpointer elem,
                              ATTR_UNUSED gpointer unused) {
    edl_item_elems *item = (edl_item_elems *) elem;
    if (item)
    {
       if(item->r)
       r_close(item->r);
    }
    g_free(elem);
}

 static void destroy_file_list_data(gpointer elem,
                              ATTR_UNUSED gpointer unused) {
	Filele *item = (Filele *) elem;
	if (item)
	{
	fnc_log(FNC_LOG_DEBUG, "free filele %s:",item->filename);
	g_free(elem);
	}
}

static void __attribute__((unused)) print_filele(gpointer elem,
                              ATTR_UNUSED gpointer unused)  {
	Filele *item = (Filele *) elem;
	if (item)
	{
		fnc_log(FNC_LOG_DEBUG, "filele name: %s, start_s:%lld, start_us:%d, end_s: %lld, end_us: %d"
				,item->filename, (long long)item->file_start_time.tv_sec, (int)item->file_start_time.tv_usec, 
				(long long)item->file_end_time.tv_sec, (int)item->file_end_time.tv_usec);
	}
}

static void change_track_parent(gpointer elem, gpointer new_parent) {
    ((Track *) elem)->parent = new_parent;
}

static double edl_timescalers (Resource * r, double res_time) {
    edl_item_elems *item = ((edl_priv_datas *)r->edl->private_data)->active->data;
    if (!item) {
        fnc_log(FNC_LOG_FATAL, "[agg] NULL edl item: This shouldn't happen");
        return HUGE_VAL;
    }
    if ((res_time + item->begin-item->file_start) >= item->end) {
	return -1.0;
    }
    //return res_time + item->offset - item->begin;
    return res_time + item->begin-item->file_start;
}

/*
*@brief retrun time1-time2
*/
#define MICROSEC 1000000
#if 0
static int timeval_GE(const struct timeval * tv1,
			 const struct timeval * tv2) {
  return (unsigned)tv1->tv_sec > (unsigned)tv2->tv_sec
    || (tv1->tv_sec == tv2->tv_sec
	&& (unsigned)tv1->tv_usec >= (unsigned)tv2->tv_usec);
}

static int timeval_equal(const struct timeval * tv1,
			 const struct timeval * tv2) {
  return (tv1->tv_sec == tv2->tv_sec
          && (unsigned)tv1->tv_usec == (unsigned)tv2->tv_usec);
}
#endif

static int timeval_great(const struct timeval * tv1,
			 const struct timeval * tv2) {
  return (unsigned)tv1->tv_sec > (unsigned)tv2->tv_sec
    || (tv1->tv_sec == tv2->tv_sec
	&& (unsigned)tv1->tv_usec > (unsigned)tv2->tv_usec);
}
#if 1
/* tv3 = tv1 + tv2 */
static void timeval_add(const struct timeval * tv1,
			 const struct timeval * tv2, struct timeval *tv3) {
    struct timeval tempTv;

    tempTv.tv_usec = (tv1->tv_usec + tv2->tv_usec) % MICROSEC;
    tempTv.tv_sec = tv1->tv_sec + tv2->tv_sec + ((tv1->tv_usec + tv2->tv_usec) / MICROSEC);
    
    *tv3 = tempTv;
}
#endif

/* tv3 = tv1 - tv2 */
static void timeval_sub(const struct timeval * tv1,
			 const struct timeval * tv2, struct timeval * tv3) {
    struct timeval tempTv;
    tempTv.tv_sec = tv1->tv_sec - tv2->tv_sec - ((tv1->tv_usec < tv2->tv_usec)?1:0);
    tempTv.tv_usec = (tv1->tv_usec < tv2->tv_usec)? 
                     (tv1->tv_usec + MICROSEC - tv2->tv_usec):
                     (tv1->tv_usec - tv2->tv_usec);

    *tv3 = tempTv;

}





static double timeval_to_double(const struct timeval * tv)
{
    if(tv == NULL) {
        return -1;
    }
    return (double)(tv->tv_sec + tv->tv_usec/1000000.0);
}



static void free_temp_file_list(GList * file_info);


static Filele *get_file_in_range(vfm_search_result_t* file_info, 
								 char * inner_base_path, 
                                 time_t start_time, 
                                 time_t end_time)

{
	Filele *file_item=NULL;
    struct timeval file_start_time, file_end_time, file_duration;

	if((file_info == NULL) ||
       (start_time == 0) || (end_time == 0)) return NULL;



	/* file start time and end time*/
	file_start_time.tv_sec = file_info->start_sec;
	file_start_time.tv_usec = file_info->start_ms * 1000;
	
	file_duration.tv_sec = file_info->duration / 1000;
	file_duration.tv_usec = (file_info->duration % 1000) * 1000;

	timeval_add(&file_start_time, &file_duration, &file_end_time);


	file_item = g_new0(Filele,1);

	snprintf(file_item->filename, FILE_PATHNAME_MAX_LEN, "%s/%s", inner_base_path, file_info->file_path);
	file_item->filename[FILE_PATHNAME_MAX_LEN - 1] = 0;


	/****************************************/
	/*                    start                            end          */
	/*   range           |_____________________|           */
	/*                              start             end               */
	/*   file                         |___________|                 */
	/*                                                                     */
	/****************************************/
	if((file_start_time.tv_sec >= start_time) && (file_end_time.tv_sec <= end_time)) 
	{
        file_item->file_start_time = file_start_time;
        file_item->file_end_time = file_end_time;
        file_item->file_offset.tv_sec = 0;
        file_item->file_offset.tv_usec = 0;
        timeval_sub(&(file_item->file_end_time), 
                    &(file_item->file_start_time), 
                    &(file_item->file_length));
		
		return file_item;
	}
	else
	/****************************************/
	/*                    start                            end          */
	/*   range           |_____________________|           */
	/*             start             end                                */
	/*   file       |___________|                                   */
	/*                                                                     */
	/****************************************/
	if((start_time > file_start_time.tv_sec) && (file_end_time.tv_sec > start_time) && 
	   (end_time >= file_end_time.tv_sec))
	{

        file_item->file_start_time.tv_sec = start_time;
        file_item->file_start_time.tv_usec = 0;
        file_item->file_end_time = file_end_time;

        timeval_sub(&file_item->file_start_time, &file_start_time, &(file_item->file_offset));

        timeval_sub(&(file_item->file_end_time), 
                    &(file_item->file_start_time), 
                    &(file_item->file_length));

		return file_item;
	}
	else
	/****************************************/
	/*                    start                            end          */
	/*   range           |_____________________|           */
	/*                                            start             end */
	/*   file                                       |___________|  */
	/*                                                                     */
	/****************************************/
	if((end_time > file_start_time.tv_sec) && (file_start_time.tv_sec >= start_time) && (file_end_time.tv_sec > end_time)) 
	{
        file_item->file_start_time = file_start_time;
		file_item->file_end_time.tv_sec = end_time;
		file_item->file_end_time.tv_usec = 0;

        file_item->file_offset.tv_sec = 0;
        file_item->file_offset.tv_usec = 0;

        timeval_sub(&(file_item->file_end_time), 
                    &(file_item->file_start_time), 
                    &(file_item->file_length));

		return file_item;
	}
	else
	/****************************************/
	/*                    start                            end          */
	/*   range           |_____________________|           */
	/*             start                                             end */
	/*   file       |________________________________|  */
	/*                                                                     */
	/****************************************/	
	if((start_time > file_start_time.tv_sec) && (file_end_time.tv_sec > end_time)) 
    {


		file_item->file_start_time.tv_sec = start_time;
		file_item->file_start_time.tv_usec = 0;

		file_item->file_end_time.tv_sec = end_time;
		file_item->file_end_time.tv_usec = 0;

        timeval_sub(&file_item->file_start_time, &file_start_time, &(file_item->file_offset));

        timeval_sub(&(file_item->file_end_time), 
                    &(file_item->file_start_time), 
                    &(file_item->file_length));

		return file_item;
	}
	else
	/****************************************/               /****************************************/
	/*                    start             end                        */               /*                                            start             end */
	/*   range           |___________|                          */               /*   range                                  |___________|    */
	/*                                            start            end         or 	                  start            end                                */
	/*   file                                       |_________|     */			/*   file            |_________|                                 */
	/*                                                                    */               /*                                                                     */
	/****************************************/              /****************************************/
	if((file_start_time.tv_sec >= end_time) || (start_time >= file_end_time.tv_sec)) 
	{
		g_free(file_item);
		file_item = NULL;
		return NULL;
	}

    g_free(file_item);
    file_item = NULL;

	return file_item;
}

#define g_list_for_each_entry_safe(node, next, head)			\
	for (node = head,	\
	     next = (node!=NULL)?g_list_next(node):NULL;	\
	     node != NULL; 					\
	     node = next, next = (node!=NULL)?g_list_next(node):NULL)




vfm_search_result_t * search_vfm_files(const char *base_path, 
										const char *db_path_name, 
										const char *ipc_uuid,
										long long start, 
										long long end)
{
	vfm_conf_t vfm_conf_v;
	vfm_hdlr_t *vfm_hdr_v;
	int ret;
	vfm_search_result_t *vfm_result_list = NULL;
	long long adjust_start;

	memset(&vfm_conf_v, 0, sizeof(vfm_conf_v));
	vfm_conf_v.meta_type = VFM_META_SQLITE3;
	strncpy(vfm_conf_v.meta_path, db_path_name, VFM_MAX_PATH_SIZE);
	strncpy(vfm_conf_v.storage_path, base_path, VFM_MAX_PATH_SIZE);


	ret = vfm_init(&vfm_conf_v, &vfm_hdr_v);
	if(ret) {
		fnc_log(FNC_LOG_ERR,"open vfm fail for %s\n", db_path_name);
		return NULL;
	}
	
	// fast path
#define FAST_TRHESHOLD  6000


	adjust_start = start - FAST_TRHESHOLD; // reduce start time for 10000 sec to search backward 
	if(adjust_start < 0) {
		adjust_start = 0;
	}

	ret = vfm_search_file(vfm_hdr_v, ipc_uuid, adjust_start, end, 0, &vfm_result_list);
	if(ret) {
		fnc_log(FNC_LOG_ERR,"search vfm fail(path:%s, uuid:%s, start:%lld, end:%lld)\n", 
				db_path_name, ipc_uuid, adjust_start, end);
		goto out;
	}

out:
	vfm_deinit(vfm_hdr_v);
	return vfm_result_list;

}


/**
 * @brief get file name list between start_time and end_time 
 *
 */
static GList* get_file_list(Resource * r, 
                            const char *base_path, 
                            const char *db_path_name, 
                            const char *ipc_uuid,
                            long long start, 
							long long end)
{
	vfm_search_result_t * vfm_result_list, *next_vfm_result;
	GList *global_file_list_head=NULL;
	GList *file_list_head=NULL;
	Filele * temp_file = NULL;
    struct timeval last_time_tv;
	char inner_base_path[FILE_PATHNAME_MAX_LEN];


	/* search db */
	vfm_result_list = search_vfm_files(base_path,db_path_name,ipc_uuid,start,end);
	if(vfm_result_list == NULL) {
		return NULL;
	}
	
	memset(inner_base_path, 0, FILE_PATHNAME_MAX_LEN);
	get_inner_path(r, inner_base_path, base_path);

    /* filter file in the time range */
	while(vfm_result_list) 
    {
        temp_file=get_file_in_range(vfm_result_list, inner_base_path, (time_t)start, (time_t)end);
        if(temp_file){
            global_file_list_head = g_list_prepend(global_file_list_head, temp_file);
        }
		next_vfm_result = vfm_result_list->next;
        free(vfm_result_list);
		vfm_result_list = next_vfm_result;
    }


    /* reverse the list to make alphasort */
    if(global_file_list_head) {
        global_file_list_head = g_list_reverse(global_file_list_head);
    }

	/* for debug */
	/*
	g_list_foreach(global_file_list_head, print_filele, NULL);
    */

    /* scan list to find out the continous chunk */
    while(1) {
        GList *cur_node, *next_node;
        int got_list = 0;

        if(global_file_list_head == NULL) {
            break;
        }
        temp_file = (Filele *)global_file_list_head->data;
		if(temp_file->file_start_time.tv_sec != (time_t)start) {
			break;
		}


        last_time_tv = temp_file->file_start_time;
        file_list_head = NULL;
        got_list = 0;

        g_list_for_each_entry_safe(cur_node, next_node, global_file_list_head)
		{
            temp_file = (Filele *)cur_node->data;
            if(timeval_great(&last_time_tv, &(temp_file->file_start_time))) {
                /* next file */
                continue;

            }else if(timeval_great(&(temp_file->file_start_time), &last_time_tv)){
                /* this is not a proper chunk */
                break;
            }

            /* move the node from global list to file list */
            global_file_list_head = g_list_delete_link(global_file_list_head, cur_node);
            file_list_head = g_list_prepend(file_list_head, temp_file);


            /* update last_time_tv*/
            last_time_tv = temp_file->file_end_time;
			if(last_time_tv.tv_sec >= (time_t)end){ 
                file_list_head = g_list_reverse(file_list_head);
                got_list = 1;
                break;

            }

		}

        if(got_list) {
            /* we get the correct chunk */
            break; 
        }else{

            if(file_list_head != NULL) {
               free_temp_file_list(file_list_head);
               file_list_head = NULL; 
            }
        }

    }

    /* free global list */
    if(global_file_list_head != NULL) {
        free_temp_file_list(global_file_list_head);
        global_file_list_head = NULL; 
    }

	return file_list_head;
}

static void free_temp_file_list(GList * file_info)
{
	if(file_info){
	g_list_foreach(file_info, destroy_file_list_data, NULL);
	g_list_free(file_info);
	}
}

static  GList * alloc_temp_file_list(Resource * r)
{
	char *tkn, *saveptr, *params;
	char temp_file_path[FILE_PATHNAME_MAX_LEN];
	char base_path[FILE_PATHNAME_MAX_LEN],db_path_name[FILE_PATHNAME_MAX_LEN], 
		ipc_uuid[FILE_PATHNAME_MAX_LEN];

	GList *head;
    int line_num;
	long long start_time, end_time;

    strncpy(temp_file_path, r->info->mrl, FILE_PATHNAME_MAX_LEN);
    temp_file_path[FILE_PATHNAME_MAX_LEN - 1] = 0;

	/* get db file name and base path */
    tkn = strstr(temp_file_path, ".db");
	if (tkn == NULL) {
		line_num = __LINE__;
		goto file_name_parse_err;
	}
	if (tkn[3] == '/' && tkn[4] != 0) {
		params = &(tkn[4]);
	}else{
		line_num = __LINE__;
		goto file_name_parse_err;
	}
	tkn[3] = 0; /* after taht, temp_file_path point to the db path name*/
	strncpy(db_path_name, temp_file_path, FILE_PATHNAME_MAX_LEN);
	get_file_path(base_path, db_path_name);
	fnc_log(FNC_LOG_DEBUG,"db file path name :%s\n", db_path_name);

 
	/* get ipc uuid */
	tkn=strtok_r(params, "/", &saveptr);
    if(tkn == NULL ) {
        line_num = __LINE__;
        goto file_name_parse_err;
    }
	strncpy(ipc_uuid, tkn, FILE_PATHNAME_MAX_LEN);
	fnc_log(FNC_LOG_DEBUG,"ipc uuid:%s\n", ipc_uuid);

	/* get start time and end time */
	tkn=strtok_r(NULL, "/", &saveptr);
    if(tkn == NULL ) {
        line_num = __LINE__;
        goto file_name_parse_err;
    }
	if (sscanf(tkn, "%lld_%lld", &start_time, &end_time) != 2) {
        line_num = __LINE__;
        goto file_name_parse_err;
	}
	fnc_log(FNC_LOG_DEBUG,"start_end time:%s\n", tkn);


	head=get_file_list(r, base_path, db_path_name, ipc_uuid, start_time, end_time);
	if(head ==NULL)
	{
        fnc_log(FNC_LOG_ERR,"%s: get file list failed\n", r->info->mrl);
	}
	return head;

file_name_parse_err:

    fnc_log(FNC_LOG_ERR,"Error: File path parse error for %s (line:%d)\n", 
            r->info->mrl, line_num);    
    return NULL;
}

static void edl_buffer_switcher(edl_item_elems *new_item,edl_item_elems *old_item) {
    GList *p, *q;
    int old_ser,new_ser; 
    if (!new_item ||new_item == old_item)
        return;
    old_ser = old_item->index;
    new_ser = new_item->index;
    p = old_item->r->tracks;
    q = new_item->r->tracks;
    while (p && q) {
        Track *pt = (Track *)p->data;
        Track *qt = (Track *)q->data;
        bq_producer_unref(qt->producer);
        qt->producer = pt->producer;
        if(new_ser != 0)
        qt->producer_freed = 1;
        if(old_ser != 0)
        {
           pt->producer = NULL;
        }
        p = g_list_next(p);
        q = g_list_next(q);
    }

}

static int open_and_seek_file(GList *newActive, double seek_time,Resource * r)
{
    edl_item_elems *new_item = NULL, *old_item = NULL;
    int ret;
    if(newActive == NULL){
        return RESOURCE_EOF;
    }
    new_item = (edl_item_elems *)(newActive->data);
    old_item = (edl_item_elems *)(((edl_priv_datas *) r->private_data)->active->data);
    if(new_item == NULL || old_item == NULL)
    {
        return RESOURCE_DAMAGED;
    }

    if(new_item != old_item)
    {
     
        if(!(new_item->r))
        {
            if(!(new_item->r = r_open(r->srv, new_item->filename)))
                return RESOURCE_DAMAGED;
            new_item->r->timescaler = edl_timescalers;
            new_item->r->edl = r;
            new_item->r->rtsp_sess = r->rtsp_sess;
            g_list_foreach(new_item->r->tracks, change_track_parent, r);
            edl_buffer_switcher(new_item,old_item);
        }

        if((old_item->index)&&(old_item->r))
        {
            r_close(old_item->r);
            old_item->r = NULL;
        }        
    }

    ret = new_item->r->demuxer->seek(new_item->r,seek_time);
    ((edl_priv_datas *) r->private_data)->active = newActive;

	return ret;
}

static int agg_probe(const char *filename)
{
    if ( strstr(filename, ".db") == NULL)
        return RESOURCE_DAMAGED;

    return RESOURCE_OK;
}
static int agg_init(Resource * r)
{
    double r_offset = 0.0;
    gint n=0;
    Resource *resource;
    GList *edl_head = NULL,*temp_node=NULL;
    edl_item_elems *item = NULL;
    feng *srv = r->srv;
    Filele *file;
    double duration;


    GList *temp_list_head = alloc_temp_file_list(r);
    fnc_log(FNC_LOG_DEBUG, "[agg] EDL init function");
    if(!temp_list_head)
    {
     return ERR_ALLOC;
    }
    temp_node = temp_list_head;
    while(temp_node)
    {
          
        file= (Filele *)temp_node->data;
        item = g_new0(edl_item_elems, 1);
        memset(item, 0, sizeof(edl_item_elems));
        /* Init Resources required by the EDitList
         * (modifying default behaviour to manipulate timescale)
         * */
        strcpy(item->filename,file->filename);

        duration = timeval_to_double(&(file->file_length));
        item->begin = r_offset;
        item->end = item->begin+duration;
        item->file_start = timeval_to_double(&(file->file_offset));
        r_offset += duration;

        if(n == 0) // for first real resource 
        {
            if (!(resource = r_open(srv, item->filename)))
            {
           	   goto err_alloc;
            }
            resource->timescaler = edl_timescalers;
            resource->edl = r;
            resource->rtsp_sess = r->rtsp_sess;
            item->r = resource;

			assert(r->tracks == NULL);

			// Use first resource for tracks
            r->tracks = resource->tracks;
            g_list_foreach(resource->tracks, change_track_parent, r);
        }

        item->index = n;

        //seek to begin
        fnc_log(FNC_LOG_DEBUG, "file begin %f\n",item->begin);
        fnc_log(FNC_LOG_DEBUG, "file end %f\n",item->end);
        //  fnc_log(FNC_LOG_DEBUG, "file duration %f\n",resource->info->duration);
        edl_head = g_list_prepend(edl_head, item);

        temp_node = g_list_next(temp_node);
        n++;
    }
    r->private_data = g_new0(edl_priv_datas, 1);
    r->info->duration = r_offset;
    ((edl_priv_datas *) r->private_data)->head = g_list_reverse(edl_head);
    ((edl_priv_datas *) r->private_data)->active = g_list_first(((edl_priv_datas *) r->private_data)->head);
    free_temp_file_list(temp_list_head);
    temp_list_head = NULL;
    temp_node = NULL;


    return RESOURCE_OK;


err_alloc:
    if(item != NULL) {
        g_free(item);
        item =NULL;
    }

    if(temp_list_head != NULL) {
        free_temp_file_list(temp_list_head);
        temp_list_head = NULL;
        temp_node = NULL;
    }

    if(edl_head != NULL) {
      g_list_foreach(edl_head, destroy_list_data, NULL);
      g_list_free(edl_head);
      if(r->private_data != NULL) {
          g_free(r->private_data);
          r->private_data = NULL;
      }
    }


    return ERR_ALLOC;
}

static int agg_read_packet(Resource * r)
{
	edl_priv_datas *data = ((edl_priv_datas *) r->private_data);
	edl_item_elems *item;
	int res;
	fnc_log(FNC_LOG_VERBOSE, "[agg] EDL read_packet function");
	if (!(data->active)) 
	{
		return RESOURCE_EOF;
	}
	else
	{
		item = (edl_item_elems *)(data->active->data);
	}
	
	while ((res = item->r->demuxer->read_packet(item->r)) == RESOURCE_EOF) 
	{
        GList *nextActive = g_list_next(data->active);
		if(nextActive)
		{
			item = (edl_item_elems *)nextActive->data;
			res = open_and_seek_file(nextActive,item->file_start,r);
			if(res) {
				return res;
			}
		}
		else
		{
			return RESOURCE_EOF;
		}
	}
	return res;
}

/* Define the â€œds_seekâ€?macro to NULL so that FNC_LIB_DEMUXER will
 * pick it up and set it to NULL. This actually saves us from having
 * to devise a way to define non-seekable demuxers.
 */
static int agg_seek(Resource * r, double time_sec)
{
	int ret = RESOURCE_NOT_FOUND;
	double seek_time;
	fnc_log(FNC_LOG_DEBUG, "range begin %f\n",time_sec);
	edl_priv_datas *res_data=(edl_priv_datas *)(r->private_data);
	edl_item_elems* res_item;
	//edl_item_elems* old_item = (edl_item_elems *)res_data->active->data;
	GList *node= res_data->head;
	
		while(node)
		{
			res_item = (edl_item_elems *)node->data;
			if((time_sec>= res_item->begin)&&(time_sec <= res_item->end))
			{
				seek_time = time_sec - res_item->begin + res_item->file_start;
				fnc_log(FNC_LOG_DEBUG, "seek time %f\n",seek_time);
                ret = open_and_seek_file(node,seek_time,r);
                if(ret == RESOURCE_OK)
                {
                    r->lastTimestamp = time_sec;
                }
                return ret;
			}
			node = g_list_next(node);
		}
		return RESOURCE_NOT_PARSEABLE;
}

static void agg_uninit(gpointer rgen)
{
    Resource *r = rgen;
    GList *edl_head = NULL;
    if (r->private_data){
        edl_head = ((edl_priv_datas *) r->private_data)->head;
    
        if (edl_head) {
            g_list_foreach(edl_head, destroy_list_data, NULL);
            g_list_free(edl_head);

        }

        g_free(r->private_data);
        r->private_data = NULL;

    }
    r->tracks = NULL; //Unlink local copy of first resource tracks
}

FNC_LIB_DEMUXER(agg);
#endif

