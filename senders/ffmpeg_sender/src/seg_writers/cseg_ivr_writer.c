/**
 * This file is part of ffmpeg_sender, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/



#include <float.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>


#include "libavutil/avstring.h"
#include "libavutil/opt.h"



#include "libavformat/avformat.h"
    
#include "../stsw_cached_segment.h"

static int create_file(char * ivr_rest_uri,
                       CachedSegment *segment, 
                       char * filename, filename_size,
                       char * file_uri, file_uri_size)
{
    uint8_t checksum[16];
    av_md5_sum(checksum, segment->buffer, segment->size);
    
}

static int upload_file(CachedSegment *segment, 
                       char * file_uri)
{
    
}

static void save_file(char * ivr_rest_uri;
                      CachedSegment *segment, 
                      char * filename)
{
    
}

static int ivr_init(CachedSegmentContext *cseg)
{
    //nothing to init
    return 0;
}


#define MAX_FILE_NAME 1024
#define MAX_URI_LEN 1024
static int ivr_write_segment(CachedSegmentContext *cseg, CachedSegment *segment)
{
    char *ivr_rest_uri = NULL;
    char file_uri[MAX_URI_LEN];
    char filename[MAX_FILE_NAME];
    char ext_name[32] = "";
    char *p;
    AVIOContext *file_context;
    int ret = 0;
    uint8_t *buf;

    
    av_strlcpy(base_name, cseg->filename, MAX_FILE_NAME);
    p = strchr(cseg->filename, ':');  
    if(p){
        ivr_rest_uri = av_asprintf("http:%s", p + 1);
    }else{
        ret = AVERROR(EINVAL);
        goto fail;
    }
    
    //get URI of the file for segment
    ret = create_file(ivr_rest_uri, segment, 
                      filename, MAX_FILE_NAME,
                      file_uri, MAX_URI_LEN);
    if(ret){
        goto fail;
    }
    
    //upload segment to the file URI
    ret = upload_file(segment, file_uri);                      
    if(ret){
        goto fail;
    }    
    
    //save the file info to IVR db
    ret = save_file(ivr_rest_uri, segment, filename);
    if(ret){
        goto fail;
    }    


fail:
    av_free(ivr_rest_uri);
    
    return ret;
}

static void ivr_uninit(CachedSegmentContext *cseg)
{
   
}
CachedSegmentWriter cseg_file_writer = {
    .name           = "ivr_writer",
    .long_name      = "IVR cloud storage segment writer", 
    .protos         = "ivr", 
    .init           = ivr_init, 
    .write_segment  = ivr_write_segment, 
    .uninit         = ivr_uninit,
};

