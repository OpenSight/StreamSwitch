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

#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "demuxer.h"
#include "feng.h"
#include "fnc_log.h"

// global demuxer modules:
extern Demuxer fnc_demuxer_stsw;



/**
 * @defgroup resources
 * @brief Resource handling functions
 *
 * @{
 */

/**
 * @brief Free a resource object
 *
 * @param resource The resource to free
 * @param user_data Unused
 *
 * @internal Please note that this function will execute the freeing
 *           in the currently running thread, synchronously. It should
 *           thus only be called by the functions that wouldn't block
 *           the mainloop.
 *
 * @note Nobody may hold the @ref Resource::lock mutex when calling
 *       this function.
 *
 * @note This function uses two parameters because it's used as
 *       interface for the closing thread pool.
 */
static void r_free_cb(gpointer resource_p,
                      gpointer user_data)
{
    Resource *resource = (Resource *)resource_p;
    if (!resource)
        return;


    if (resource->lock)
        g_mutex_free(resource->lock);


    fnc_log(FNC_LOG_DEBUG, "close resource %s:",resource->info->name);

    g_free(resource->info->mrl);
    g_free(resource->info->name);
    g_slice_free(ResourceInfo, resource->info);

    resource->info = NULL;
    resource->demuxer->uninit(resource);

    if (resource->tracks) {
        g_list_foreach(resource->tracks, free_track, NULL);
        g_list_free(resource->tracks);
    }
    g_slice_free(Resource, resource);
}

/**
 * @brief Find the correct demuxer for the given resource.
 *
 * @param filename Name of the file (to find the extension)
 *
 * @return A constant pointer to the working demuxer.
 *
 * This function first tries to match the resource extension with one
 * of those served by the demuxers, that will be probed; if this fails
 * it tries every demuxer available with direct probe.
 *
 * */

static const Demuxer *r_find_demuxer(const char *filename)
{
    static const Demuxer *const demuxers[] = {
        &fnc_demuxer_stsw,
        NULL
    };

    int i;
    const char *res_ext;

    /* First of all try that with matching extension: we use extension as a
     * hint of resource type.
     */
    if ( (res_ext = strrchr(filename, '.')) && *(res_ext++) ) {
        for (i=0; demuxers[i]; i++) {
            char exts[128], *tkn; /* temp string containing extension
                                   * served by probing demuxer.
                                   */
            strncpy(exts, demuxers[i]->info->extensions, sizeof(exts)-1);
            
            if(demuxers[i]->info->fake_path || strlen(exts) == 0){
                /* no file extension for this demuxer */
                continue;
            }
            

            for (tkn=strtok(exts, ","); tkn; tkn=strtok(NULL, ",")) {
                
                if (strcmp(tkn, res_ext) == 0)
                    continue;

                fnc_log(FNC_LOG_DEBUG, "[MT] probing demuxer: \"%s\" "
                        "matches \"%s\" demuxer\n", res_ext,
                        demuxers[i]->info->name);

                if (demuxers[i]->probe(filename) == RESOURCE_OK)
                    return demuxers[i];
            }
        }
    }

    /* try fake path demuxer first */

    for (i=0; demuxers[i]; i++){
        if(demuxers[i]->info->fake_path != 0){

            if ((demuxers[i]->probe(filename) == RESOURCE_OK))
                return demuxers[i];
        }
    }

    for (i=0; demuxers[i]; i++){
        if(demuxers[i]->info->fake_path == 0){

            if ((demuxers[i]->probe(filename) == RESOURCE_OK))
                return demuxers[i];
        }
    }

    return NULL;
}

/**
 * @brief Open a new resource and create a new instance
 *
 * @param srv The server object for the vhost requesting the resource
 * @param inner_path The path, relative to the avroot, for the
 *                   resource
 *
 * @return A new Resource object
 * @retval NULL Error while opening resource
 */
Resource *r_open(struct feng *srv, const char *inner_path)
{
    Resource *r;

    const Demuxer *dmx;

    
    gchar *mrl = g_strjoin ("/",
                            srv->config_storage.document_root->ptr,
                            inner_path,
                            NULL);
	//struct stat filestat;

    /* Jamken: don't check if mrl is a file */
#if 0
	if (stat(mrl, &filestat) == -1 ) {
		switch(errno) {
        case ENOENT:
            fnc_log(FNC_LOG_ERR,"%s: file not found\n", mrl);
            break;
        default:
            fnc_log(FNC_LOG_ERR,"Cannot stat file %s\n", mrl);
            break;
		}
        goto error;
	}

	if ( S_ISFIFO(filestat.st_mode) ) {
		fnc_log(FNC_LOG_ERR, "%s: not a file\n");
        goto error;
    }
#endif

    if ( (dmx = r_find_demuxer(mrl)) == NULL ) {
        fnc_log(FNC_LOG_DEBUG,
                "[MT] Could not find a valid demuxer for resource %s\n",
                mrl);
        goto error;
    }


    fnc_log(FNC_LOG_DEBUG, "[MT] registrered demuxer \"%s\" for resource"
                               "\"%s\"\n", dmx->info->name, mrl);

    r = g_slice_new0(Resource);
    
    
    r->info = g_slice_new0(ResourceInfo);

    /* init some field of ResourceInfo, 
     * but demuxer->init method may change it 
     */
    r->info->mrl = mrl;
    r->info->mtime = 0;
    r->info->name = g_strdup(inner_path);
    r->info->seekable = (dmx->seek != NULL);


    r->demuxer = dmx;
    r->srv = srv; 
    r->rtsp_sess = NULL;
    r->model = MM_PULL;
    
    r->lock = g_mutex_new();

    
        
    if (r->demuxer->init(r)) {
        r_free_cb(r, NULL);
        return NULL;
    }

    /* Now that we have opened the actual resource we can proceed with
     * the extras */
    fnc_log(FNC_LOG_DEBUG, "init resource %s:",r->info->name);

    return r;
 error:
    g_free(mrl);
    return NULL;
}

/**
 * @brief Comparison function to compare a Track to a name
 *
 * @param a Track pointer to the element in the list
 * @param b String with the name to compare to
 *
 * @return The strcmp() result between the correspondent Track name
 *         and the given name.
 *
 * @internal This function should _only_ be used by @ref r_find_track.
 */
static gint r_find_track_cmp_name(gconstpointer a, gconstpointer b)
{
    return strcmp( ((Track *)a)->info->name, (const char *)b);
}

/**
 * @brief Find the given track name in the resource
 *
 * @param resource The Resource object to look into
 * @param track_name The name of the track to look for
 *
 * @return A pointer to the Track object for the requested track.
 *
 * @retval NULL No track in the resource corresponded to the given
 *              parameters.
 *
 * @todo This only returns the first resource corresponding to the
 *       parameters.
 * @todo Capabilities aren't used yet.
 */
Track *r_find_track(Resource *resource, const char *track_name) {
    GList *track = g_list_find_custom(resource->tracks,
                                      track_name,
                                      r_find_track_cmp_name);

    if ( !track ) {
        fnc_log(FNC_LOG_DEBUG, "Track %s not present in resource %s\n",
                track_name, resource->info->mrl);
        return NULL;
    }

    return track->data;
}



/**
 * @brief Seek a resource to a given time in stream
 *
 * @param resource The Resource to seek
 * @param time The time in seconds within the stream to seek to
 *
 * @return The value returned by @ref Demuxer::seek
 *
 * @note This function will lock the @ref Resource::lock mutex.
 */
int r_seek(Resource *resource, double time) {
    int res;

    g_mutex_lock(resource->lock);
    
    if(resource->demuxer->seek){
        res = resource->demuxer->seek(resource, time);
    }
    
    if (resource->tracks) {
        g_list_foreach(resource->tracks, reset_track, NULL);
    }


    g_mutex_unlock(resource->lock);

    return res;
}

/**
 * @brief Read data from a resource (unlocked)
 *
 * @param resouce The resource to read from
 *
 * @return The same return value as read_packet
 */
int r_read(Resource *resource)
{
    int ret = RESOURCE_EOF;

    g_mutex_lock(resource->lock);
    if (!resource->eor)
        switch( (ret = resource->demuxer->read_packet(resource)) ) {
        case RESOURCE_OK:
            break;
        case RESOURCE_EOF:
            fnc_log(FNC_LOG_INFO,
                    "r_read_unlocked: %s read_packet() end of file.",
                    resource->info->mrl);
            resource->eor = true;
            break;
        default:
            fnc_log(FNC_LOG_FATAL,
                    "r_read_unlocked: %s read_packet() error.",
                    resource->info->mrl);
            break;
        }

    g_mutex_unlock(resource->lock);

    return ret;
}

/**
 * @brief Request closing of a resource
 *
 * @param resource The resource to close
 *
 *
 * @see r_free_cb
 */
void r_close(Resource *resource)
{
    r_free_cb(resource,NULL);

}


/**
 * @brief start a resource for reading
 *
 * @param resouce The resource to start
 *
 * @return The same return value as demuxer->start
 */
int r_start(Resource *resource)
{
    int ret = RESOURCE_OK;
    
    g_mutex_lock(resource->lock);
    if(resource->demuxer->start){
        ret = resource->demuxer->start(resource);
    }
    if(ret == RESOURCE_OK){
        resource->eor = false;
    }else if(ret == RESOURCE_EOF){
        resource->eor = true;
    }

    g_mutex_unlock(resource->lock);    

    fnc_log(FNC_LOG_DEBUG, "start resource %s:",resource->info->name);
    
    return ret;
}



/**
 * @brief Request pause of a resource
 *
 * @param resource The resource to pause
 *
 *
 */
void r_pause(Resource *resource)
{
    g_mutex_lock(resource->lock);
    if(resource->demuxer->pause){
        resource->demuxer->pause(resource);
    }
    
    //Jamken: After pause ,there may be some packet buffered 
    // in the track's producer queue. For replay resource, 
    // it's OK because of these packet's time is irrelavant with 
    // the play range. But for live resource, these packet's time 
    // is relative to the playback_time of the former play range. 
    // so that the time of these packet current in queue vould be invalid
    // for the latter play range. just reset all tracks and their
    // queue for live stream
    if(resource->info->media_source == MS_live){
        if (resource->tracks) {
            g_list_foreach(resource->tracks, reset_track, NULL);
        }
    }

    g_mutex_unlock(resource->lock);
    
    fnc_log(FNC_LOG_DEBUG, "pause resource %s:",resource->info->name);    
}

/**
 * @}
 */

