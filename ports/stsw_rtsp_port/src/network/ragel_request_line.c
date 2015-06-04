/**
 * This file is part of stsw_rtsp_port, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2015  OpenSight team (www.opensight.cn)
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

#include "network/rtsp.h"

size_t parseRTSPRequestString(char const* reqStr,
                               unsigned reqStrSize,
                               char* resultCmdName,
                               unsigned resultCmdNameMaxSize,
                               char* resultUrl,
                               unsigned resultUrlMaxSize,
                               char* resultVersion,
                               unsigned resultVersionMaxSize) {
    unsigned lineSize = 0;
    unsigned i,j;
    int parseSucceeded = 0;


    //find out the size of request line;
    for (i = 0; i < reqStrSize; ++i) {
        if(reqStr[i] == '\n') break;
    }
    if( i ==  reqStrSize) return 0; /* parse faild */
    lineSize = i + 1;

    i = 0;

    //parse the Method 
    parseSucceeded = 0;
    j = 0;
    for (; j < resultCmdNameMaxSize-1 && i < lineSize; ++i) {
        char c = reqStr[i];
        if (c == ' ' || c == '\t') {
            parseSucceeded = 1;
            break;
        }
        
        resultCmdName[j++] = c;
    }

    resultCmdName[j++] = '\0';
    if (!parseSucceeded) return 0;

    while (i < lineSize && (reqStr[i] == ' ' || reqStr[i] == '\t')) ++i; // skip over any additional white space
    
    //parse the Request-URI 
    parseSucceeded = 0;
    j = 0;
    for (; j < resultUrlMaxSize-1 && i < lineSize; ++i) {
        char c = reqStr[i];
        if (c == ' ' || c == '\t') {
            parseSucceeded = 1;
            break;
        }

        resultUrl[j++] = c;
    }

    resultUrl[j++] = '\0';
    if (!parseSucceeded) return 0;

    while (i < lineSize && (reqStr[i] == ' ' || reqStr[i] == '\t')) ++i; // skip over any additional white space

    //parse the RTSP-Version 
    parseSucceeded = 0;
    j = 0;
    for (; j < resultVersionMaxSize-1 && i < lineSize; ++i) {
        char c = reqStr[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            parseSucceeded = 1;
            break;
        }
        resultVersion[j++] = c;
    }

    resultVersion[j++] = '\0';
    if (!parseSucceeded) return 0;
    

    return lineSize;
}
#define RTSP_PARAM_STRING_MAX 256
size_t ragel_parse_request_line(const char *msg, const size_t length, RTSP_Request *req)
{
    char cmdName[RTSP_PARAM_STRING_MAX];
    char url[RTSP_PARAM_STRING_MAX];
    char version[RTSP_PARAM_STRING_MAX];

    size_t lineSize;

    lineSize = parseRTSPRequestString(msg,length,
                                      cmdName,RTSP_PARAM_STRING_MAX,
                                      url,RTSP_PARAM_STRING_MAX,
                                      version,RTSP_PARAM_STRING_MAX);
    if( lineSize == 0 ) {
        return 0;
    }

    req->method = g_strndup(cmdName, RTSP_PARAM_STRING_MAX - 1);
    req->object = g_strndup(url, RTSP_PARAM_STRING_MAX - 1);
    req->version = g_strndup(version, RTSP_PARAM_STRING_MAX - 1);

    if( !strcmp(req->method, "DESCRIBE" )) {
        req->method_id = RTSP_ID_DESCRIBE;
    }else if( !strcmp(req->method, "ANNOUNCE" )){
        req->method_id = RTSP_ID_ANNOUNCE;

    }else if(!strcmp(req->method, "GET_PARAMETER" )){
        req->method_id = RTSP_ID_GET_PARAMETER;
    }else if(!strcmp(req->method, "OPTIONS" )){
        req->method_id = RTSP_ID_OPTIONS;

    }else if(!strcmp(req->method, "PAUSE" )){
        req->method_id = RTSP_ID_PAUSE;

    }else if(!strcmp(req->method, "PLAY" )){
        req->method_id = RTSP_ID_PLAY;

    }else if(!strcmp(req->method, "RECORD" )){
        req->method_id = RTSP_ID_RECORD;

    }else if(!strcmp(req->method, "REDIRECT" )){
        req->method_id = RTSP_ID_REDIRECT;

    }else if(!strcmp(req->method, "SETUP" )){
        req->method_id = RTSP_ID_SETUP;

    }else if(!strcmp(req->method, "SET_PARAMETER" )){
        req->method_id = RTSP_ID_SET_PARAMETER;

    }else if(!strcmp(req->method, "TEARDOWN" )){
        req->method_id = RTSP_ID_TEARDOWN;
    }else{
        req->method_id = RTSP_ID_ERROR;
    }
    
    return lineSize;
}

