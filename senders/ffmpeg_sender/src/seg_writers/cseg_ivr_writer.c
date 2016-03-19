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
#include <curl/curl.h>

#include "libavutil/avstring.h"
#include "libavutil/opt.h"

#include "libavformat/avformat.h"
    
#include "../stsw_cached_segment.h"
#include "../cJSON.h"

#define MIN(a,b) ((a) > (b) ? (b) : (a))


static int http_status_to_av_code(int status_code)
{
    if(status_code == 400){
        return AVERROR_HTTP_BAD_REQUEST;
    }else if(status_code == 404){
        return AVERROR_HTTP_NOT_FOUND;
    }else if (status_code > 400 && status_code < 500){
        return AVERROR_HTTP_OTHER_4XX;
    }else if ( status_code >= 500 && status_code < 600){
        return AVERROR_HTTP_SERVER_ERROR;
    }else{
        return AVERROR_UNKNOWN;
    }    
}


#define  IVR_NAME_FIELD_KEY  "name"
#define  IVR_URI_FIELD_KEY  "uri"
#define  IVR_ERR_INFO_FIELD_KEY "info"

#define MAX_HTTP_RESULT_SIZE  8192

#define ENABLE_CURLOPT_VERBOSE


typedef struct HttpBuf{
    char * buf;
    int buf_size; 
    int pos;    
}HttpBuf;

static size_t http_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    int data_size = size * nmemb;
    HttpBuf * http_buf = (HttpBuf *)userdata;
        
    if(data_size > (http_buf->buf_size - http_buf->pos)){
        return 0;
    }
    memcpy(http_buf->buf + http_buf->pos, ptr, data_size);
    http_buf->pos += data_size;
    return data_size;    
}

static size_t http_read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpBuf * http_buf = (HttpBuf *)userdata;
    int data_size = MIN(size * nmemb, http_buf->buf_size - http_buf->pos);    
    
    memcpy(ptr, http_buf->buf + http_buf->pos, data_size);
    http_buf->pos += data_size;
    return data_size;
}

static int http_post(char * http_uri, 
                     int32_t io_timeout,  //in milli-seconds 
                     char * post_content_type, 
                     char * post_data, int post_len,
                     int * status_code,
                     char * result_buf, int max_buf_size,
                     char * err_buf, int err_buf_size)
{
    CURL * easyhandle = NULL;
    int ret = 0;
    struct curl_slist *headers=NULL;
    char content_type_header[128];
    long status;
    HttpBuf http_buf;
    
    memset(&http_buf, 0, sizeof(HttpBuf));
    
    if(err_buf != NULL && err_buf_size != 0){
        if(err_buf_size < CURL_ERROR_SIZE){
            ret = AVERROR(EINVAL);
            snprintf(err_buf, err_buf_size - 1, "error buf is too small");
            err_buf[err_buf_size - 1] = 0;  
            return AVERROR(EINVAL);
        }
    }
    
    easyhandle = curl_easy_init();
    if(easyhandle == NULL){
        return AVERROR(ENOMEM);
    }

    if(curl_easy_setopt(easyhandle, CURLOPT_URL, http_uri)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }   
    if(post_content_type != NULL){
        memset(content_type_header, 0, 128);
        snprintf(content_type_header, 127, 
                 "Content-Type: %s", post_content_type);
        headers = curl_slist_append(headers, content_type_header);
        if(curl_easy_setopt(easyhandle, CURLOPT_HTTPHEADER, headers)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }   
    
    if(curl_easy_setopt(easyhandle, CURLOPT_POSTFIELDS, post_data)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }  
    if(curl_easy_setopt(easyhandle, CURLOPT_POSTFIELDSIZE, post_len)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }    
    if(io_timeout > 0){
        if(curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT_MS , io_timeout)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }   
    
    if(result_buf != NULL && max_buf_size != 0){
        http_buf.buf = result_buf;
        http_buf.buf_size = max_buf_size;
        http_buf.pos = 0;
        
        if(curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, http_write_callback)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
        if(curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &http_buf)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }
    
    if(err_buf != NULL && err_buf_size != 0){
        if(curl_easy_setopt(easyhandle, CURLOPT_ERRORBUFFER, err_buf)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }
    
 #ifdef ENABLE_CURLOPT_VERBOSE   
    if(curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 1)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }    
 #endif
        
    if(curl_easy_perform(easyhandle)){
        ret = AVERROR_EXTERNAL;
        goto fail;
    }
    
    if(curl_easy_getinfo(easyhandle, CURLINFO_RESPONSE_CODE, &status)){
        ret = AVERROR_EXTERNAL;
        goto fail;
    }    
    
    if(status_code){
        *status_code = status;
    }
    ret = http_buf.pos;
    
fail:    
    if(headers != NULL){
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(easyhandle);    
    return ret;
}

static int http_put(char * http_uri, 
                    int32_t io_timeout,  //in milli-seconds 
                    char * content_type, 
                    char * buf, int buf_size,
                    int * status_code,
                    char * err_buf, int err_buf_size)
{
    CURL * easyhandle = NULL;
    int ret = 0;
    struct curl_slist *headers=NULL;
    char content_type_header[128];
    char expect_header[128];
    long status;
    HttpBuf http_buf;
    
    memset(&http_buf, 0, sizeof(HttpBuf));
    
    if(err_buf != NULL && err_buf_size != 0){
        if(err_buf_size < CURL_ERROR_SIZE){
            ret = AVERROR(EINVAL);
            snprintf(err_buf, err_buf_size - 1, "error buf is too small");
            err_buf[err_buf_size - 1] = 0;  
            return AVERROR(EINVAL);
        }
    }
    
    easyhandle = curl_easy_init();
    if(easyhandle == NULL){
        return AVERROR(ENOMEM);
    }

    if(curl_easy_setopt(easyhandle, CURLOPT_URL, http_uri)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }   
    
    if(curl_easy_setopt(easyhandle, CURLOPT_UPLOAD, 1L)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }   
    
    if(content_type != NULL){
        memset(content_type_header, 0, 128);
        snprintf(content_type_header, 127, 
                 "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, content_type_header);
    }   
    //disable "Expect: 100-continue"  header
    memset(expect_header, 0, 128);
    strcpy(expect_header, "Expect:");
    headers = curl_slist_append(headers, content_type_header);     

    if(curl_easy_setopt(easyhandle, CURLOPT_HTTPHEADER, headers)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }
   
    if(curl_easy_setopt(easyhandle, CURLOPT_INFILESIZE, buf_size)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }    

    if(io_timeout > 0){
        if(curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT_MS , io_timeout)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }   
    
    if(buf != NULL && buf_size != 0){
        http_buf.buf = buf;
        http_buf.buf_size = buf_size;
        http_buf.pos = 0;
        
        if(curl_easy_setopt(easyhandle, CURLOPT_READFUNCTION, http_read_callback)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
        if(curl_easy_setopt(easyhandle, CURLOPT_READDATA, &http_buf)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }
    
    if(err_buf != NULL && err_buf_size != 0){
        if(curl_easy_setopt(easyhandle, CURLOPT_ERRORBUFFER, err_buf)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }
    
 #ifdef ENABLE_CURLOPT_VERBOSE   
    if(curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 1)){
        ret = AVERROR_EXTERNAL;
        goto fail;                
    }    
 #endif
        
    if(curl_easy_perform(easyhandle)){
        ret = AVERROR_EXTERNAL;
        goto fail;
    }
    
    if(curl_easy_getinfo(easyhandle, CURLINFO_RESPONSE_CODE, &status)){
        ret = AVERROR_EXTERNAL;
        goto fail;
    }    
    
    if(status_code){
        *status_code = status;
    }
    
fail:    
    if(headers != NULL){
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(easyhandle);    
    
    return ret;
}


static int create_file(char * ivr_rest_uri, 
                       int32_t io_timeout, 
                       CachedSegment *segment, 
                       char * filename, int filename_size,
                       char * file_uri, int file_uri_size,
                       char * err_buf, int err_buf_size)
{
    //uint8_t checksum[16];
    //char checksum_b64[32];
    //char checksum_b64_escape[128];
    char post_data_str[256];
    char * http_response_json = av_mallocz(MAX_HTTP_RESULT_SIZE);
    cJSON * json_root = NULL;
    cJSON * json_name = NULL;
    cJSON * json_uri = NULL;    
    cJSON * json_info = NULL;        
    int ret;
    int status_code = 200;
    
    if(filename_size){
        filename[0] = 0;
    }
    if(file_uri_size){
        file_uri[0] = 0;
    }    
    
    //prepare post_data
    //av_md5_sum(checksum, segment->buffer, segment->size);
    //av_base64_encode(checksum_b64, 32, checksum, 16);
    //url_encode(checksum_b64_escape, checksum_b64);
    sprintf(post_data_str, 
            "op=create&content_type=video%%2Fmp2t&size=%d&start=%.6f&duration=%.6f",
            segment->size,
            segment->start_ts, 
            segment->duration);
    
    //issue HTTP request
    ret = http_post(ivr_rest_uri, 
                    io_timeout,
                    NULL, 
                    post_data_str, strlen(post_data_str), 
                    &status_code,
                    http_response_json, MAX_HTTP_RESULT_SIZE, 
                    err_buf, err_buf_size);
    if(ret < 0){
        goto failed;       
    }
    
    //parse the result
    if(status_code >= 200 && status_code < 300){
        json_root = cJSON_Parse(http_response_json);
        if(json_root== NULL){
            ret = AVERROR(EINVAL);
            snprintf(err_buf, err_buf_size - 1, "HTTP response Json parse failed(%s)", http_response_json);
            err_buf[err_buf_size - 1] = 0;
            goto failed;
        }
        json_name = cJSON_GetObjectItem(json_root, IVR_NAME_FIELD_KEY);
        if(json_name && json_name->type == cJSON_String && json_name->valuestring){
            av_strlcpy(filename, json_name->valuestring, filename_size);
        }
        json_uri = cJSON_GetObjectItem(json_root, IVR_URI_FIELD_KEY);
        if(json_uri && json_uri->type == cJSON_String && json_uri->valuestring){
            av_strlcpy(file_uri, json_uri->valuestring, file_uri_size);
        }
    }else{
        ret = http_status_to_av_code(status_code);
        json_root = cJSON_Parse(http_response_json);
        if(json_root== NULL){
            snprintf(err_buf, err_buf_size - 1, "HTTP response json parse failed(%s)", http_response_json);
            err_buf[err_buf_size - 1] = 0;
            goto failed;
        }
        json_info = cJSON_GetObjectItem(json_root, IVR_ERR_INFO_FIELD_KEY);
        if(json_info && json_info->type == cJSON_String && json_info->valuestring){
            av_strlcpy(err_buf, json_info->valuestring, err_buf_size);
        }        
        
    }
    
failed:
    if(json_root){
        cJSON_Delete(json_root); 
        json_root = NULL;
    }
    av_free(http_response_json);  
    
    return ret;
}

static int upload_file(CachedSegment *segment, 
                       int32_t io_timeout, 
                       char * file_uri, 
                       char * err_buf, int err_buf_size)
{
    int status_code = 200;
    int ret = 0;  
    ret = http_put(file_uri, io_timeout, "video/mp2t",
                   segment->buffer, segment->size, 
                   &status_code,
                   err_buf, err_buf_size);
    if(ret < 0){
        return ret;
    }
    
    if(status_code < 200 || status_code >= 300){
        ret = http_status_to_av_code(status_code);
        snprintf(err_buf, err_buf_size - 1, "http upload file failed with status(%d)", status_code);
        err_buf[err_buf_size - 1] = 0;  
    }
    
}

static int save_file(char * ivr_rest_uri,
                      int32_t io_timeout,
                      CachedSegment *segment, 
                      char * filename,
                      char * err_buf, int err_buf_size)
{
    char post_data_str[512];  
    int status_code = 200;
    int ret = 0;
    char * http_response_json = av_mallocz(MAX_HTTP_RESULT_SIZE);
    cJSON * json_root = NULL;
    cJSON * json_info = NULL;     
    
    //prepare post_data
    sprintf(post_data_str, "op=save&name=%s&size=%d&start=%.6f&duration=%.6f",
            filename,
            segment->size,
            segment->start_ts, 
            segment->duration);
    
    //issue HTTP request
    ret = http_post(ivr_rest_uri, 
                    io_timeout,
                    NULL, 
                    post_data_str, strlen(post_data_str), 
                    &status_code,
                    http_response_json, MAX_HTTP_RESULT_SIZE, 
                    err_buf, err_buf_size); 
    if(ret < 0){
        return ret;
    }


    if(status_code < 200 || status_code >= 300){

        ret = http_status_to_av_code(status_code);
        
        json_root = cJSON_Parse(http_response_json);
        if(json_root== NULL){
            snprintf(err_buf, err_buf_size - 1, "HTTP response json parse failed(%s)", http_response_json);
            err_buf[err_buf_size - 1] = 0;            
        }else{
            json_info = cJSON_GetObjectItem(json_root, IVR_ERR_INFO_FIELD_KEY);
            if(json_info && json_info->type == cJSON_String && json_info->valuestring){
                av_strlcpy(err_buf, json_info->valuestring, err_buf_size);
            }        
            cJSON_Delete(json_root);             
        }
      
    }

    av_free(http_response_json);

    return ret;
}



static int ivr_init(CachedSegmentContext *cseg)
{
    //init curl lib
    curl_global_init(CURL_GLOBAL_ALL);    

    
    return 0;
}


#define MAX_FILE_NAME 1024
#define MAX_URI_LEN 1024

#define FILE_CREATE_TIMEOUT 10000
static int ivr_write_segment(CachedSegmentContext *cseg, CachedSegment *segment)
{
    char ivr_rest_uri[MAX_URI_LEN] = "http";
    char file_uri[MAX_URI_LEN];
    char filename[MAX_FILE_NAME];
    char *p;
    int ret = 0;

    if(cseg->filename == NULL || strlen(cseg->filename) == 0){
        ret = AVERROR(EINVAL);
        sprintf(cseg->consumer_err_str, "http filename absent");
        goto fail;       
    }
    
    if(strlen(cseg->filename) > (MAX_URI_LEN - 5)){
        ret = AVERROR(EINVAL);
        sprintf(cseg->consumer_err_str, "filename is too long");
        goto fail;
    }

    p = strchr(cseg->filename, ':');  
    if(p){
        av_strlcat(ivr_rest_uri, p, MAX_URI_LEN);
    }else{
        ret = AVERROR(EINVAL);
        sprintf(cseg->consumer_err_str, "filename malformat");
        goto fail;
    }
    
    //get URI of the file for segment
    ret = create_file(ivr_rest_uri, 
                      FILE_CREATE_TIMEOUT,
                      segment, 
                      filename, MAX_FILE_NAME,
                      file_uri, MAX_URI_LEN,
                      cseg->consumer_err_str, CONSUMER_ERR_STR_LEN);
    if(ret){
        goto fail;
    }
    
    if(strlen(filename) == 0 || strlen(file_uri) == 0){
        ret = 1; //cannot upload at the moment
    }else{    
        //upload segment to the file URI
        ret = upload_file(segment, 
                          cseg->writer_timeout,
                          file_uri,
                          cseg->consumer_err_str, CONSUMER_ERR_STR_LEN);                      
        if(ret){
            goto fail;
        }    
        
        //save the file info to IVR db
        ret = save_file(ivr_rest_uri, 
                        FILE_CREATE_TIMEOUT,
                        segment, filename,
                        cseg->consumer_err_str, CONSUMER_ERR_STR_LEN);
        if(ret){
            goto fail;
        }  
    }  


fail:
   
    return ret;
}

static void ivr_uninit(CachedSegmentContext *cseg)
{
    curl_global_cleanup();
}


CachedSegmentWriter cseg_ivr_writer = {
    .name           = "ivr_writer",
    .long_name      = "IVR cloud storage segment writer", 
    .protos         = "ivr", 
    .init           = ivr_init, 
    .write_segment  = ivr_write_segment, 
    .uninit         = ivr_uninit,
};

