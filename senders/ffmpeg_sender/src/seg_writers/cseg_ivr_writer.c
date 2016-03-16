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


/* Converts a hex character to its integer value */
char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}


static int url_encode(char *dest, const char *src) {
    /* urlencode all non-alphanumeric characters in the C-string 'src'
       store result in the C-string 'dest'
       return the length of the url encoded C-string
    */
    char *d;
    int i;
    for(i=0, d=dest; src[i]; i++) {
        if(isalnum(src[i]) || src[i] == '-' || src[i] == '_' ||
           src[i] == '.' || src[i] == '~') {
               *(d++) = src[i];
           }
        else {
            sprintf(d, "%%%02X", src[i]);
            d += 3;
        }
    }   
    *d = 0;
    return d-dest;
}

static int hex_encode(char * dest, const char * src)
{
    int i;
    for(i=0, d=dest; src[i]; i++, d += 2){
        sprintf(d, "%02hhX", src[i]);
    }
    *d = 0;
    return d-dest;    
}

#define  IVR_OP_FIELD_KEY  "OP"
#define  IVR_NAME_FIELD_KEY  "name"
#define  IVR_URI_FIELD_KEY  "uri"
#define  IVR_SIZE_FIELD_KEY  "size"
#define  IVR_CHECKSUM_FIELD_KEY "checksum"
#define  IVR_START_TIME_FIELD_KEY "start"
#define  IVR_DURATION_FIELD_KEY "duration"

#define MAX_HTTP_RESULT_SIZE  8192

static int http_post(char * ivr_rest_uri, 
                     int32_t io_timeout,  //in milli-seconds 
                     char * post_content_type, 
                     char * post_data, int post_len,
                     char * result_buf, int max_buf_size)
{
    return 0;
}

static int http_put(char * ivr_rest_uri, 
                     int32_t io_timeout,  //in milli-seconds 
                     char * content_type, 
                     char * buf, int buf_size)
{
    return 0;
}


static int create_file(char * ivr_rest_uri,
                       CachedSegment *segment, 
                       char * filename, int filename_size,
                       char * file_uri, int file_uri_size)
{
    uint8_t checksum[16];
    char checksum_b64[32];
    char checksum_b64_escape[128];
    char post_data_str[256];
    char * http_response_json = av_mallocz(MAX_HTTP_RESULT_SIZE);
    cJSON * json_root = NULL;
    cJSON * json_name = NULL;
    cJSON * json_uri = NULL;    
    int ret;
    
    if(filename_size){
        filename[0] = 0;
    }
    if(file_uri_size){
        file_uri[0] = 0;
    }    
    
    //prepare post_data
    av_md5_sum(checksum, segment->buffer, segment->size);
    av_base64_encode(checksum_b64, 32, checksum, 16);
    url_encode(checksum_b64_escape, checksum_b64);
    sprintf(post_data_str, "op=create&size=%d&checksum=%s",
            segment->size, 
            checksum_b64_escape);
    
    //issue HTTP request
    ret = http_post(ivr_rest_uri, 
                    "application/x-www-form-urlencoded", 
                    post_data_str, strlen(post_data_str), 
                    http_response_json, MAX_HTTP_RESULT_SIZE);
    if(ret < 0){
        
    }
    
    
    
    //parse the result
    json_root = cJSON_Parse(http_response_json);
    if(json_root== NULL){
        
    }
    json_name = cJSON_GetObjectItem(json_root, IVR_NAME_FIELD_KEY);
    if(json_name){
        av_strlcpy(filename, json_name->valuestring, filename_size);
    }
    json_uri = cJSON_GetObjectItem(json_root, IVR_DURATION_FIELD_KEY);
    if(json_uri){
        av_strlcpy(file_uri, json_uri->valuestring, file_uri_size);
    }    
    
fail:    
    if(json_root){
        cJSON_Delete(json_root); 
        json_root = NULL;
    }
    av_free(http_response_json);  

    
    return 0;
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

