#line 1 "network/ragel_request_line.rl"
/* -*- c -*- */
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

#include "network/rtsp.h"

#ifdef TRISOS
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

#else

#line 27 "network/ragel_request_line.rl"

size_t ragel_parse_request_line(const char *msg, const size_t length, RTSP_Request *req) {
    int cs;
    const char *p = msg, *pe = p + length + 1, *s = NULL, *eof = pe;

    /* We want to express clearly which versions we support, so that we
     * can return right away if an unsupported one is found.
     */

    
#line 39 "network/ragel_request_line.c"
static const char _rtsp_request_line_actions[] = {
	0, 1, 0, 1, 1, 1, 8, 1, 
	9, 1, 10, 2, 0, 9, 2, 2, 
	1, 2, 3, 1, 2, 4, 1, 2, 
	5, 1, 2, 6, 1, 2, 7, 1
	
};

static const short _rtsp_request_line_key_offsets[] = {
	0, 0, 9, 14, 17, 28, 31, 43, 
	50, 54, 61, 65, 66, 72, 78, 84, 
	90, 96, 102, 108, 113, 119, 125, 131, 
	137, 143, 149, 154, 161, 167, 173, 179, 
	184, 190, 196, 201, 207, 213, 219, 225, 
	230, 236, 242, 248, 254, 260, 266, 272, 
	277, 277
};

static const char _rtsp_request_line_trans_keys[] = {
	68, 79, 80, 83, 84, 65, 90, 97, 
	122, 32, 65, 90, 97, 122, 32, 33, 
	126, 32, 33, 64, 65, 90, 91, 96, 
	97, 122, 123, 126, 32, 33, 126, 32, 
	47, 33, 64, 65, 90, 91, 96, 97, 
	122, 123, 126, 32, 33, 47, 48, 57, 
	58, 126, 32, 46, 33, 126, 32, 33, 
	47, 48, 57, 58, 126, 13, 32, 33, 
	126, 10, 32, 69, 65, 90, 97, 122, 
	32, 83, 65, 90, 97, 122, 32, 67, 
	65, 90, 97, 122, 32, 82, 65, 90, 
	97, 122, 32, 73, 65, 90, 97, 122, 
	32, 66, 65, 90, 97, 122, 32, 69, 
	65, 90, 97, 122, 32, 65, 90, 97, 
	122, 32, 80, 65, 90, 97, 122, 32, 
	84, 65, 90, 97, 122, 32, 73, 65, 
	90, 97, 122, 32, 79, 65, 90, 97, 
	122, 32, 78, 65, 90, 97, 122, 32, 
	83, 65, 90, 97, 122, 32, 65, 90, 
	97, 122, 32, 65, 76, 66, 90, 97, 
	122, 32, 85, 65, 90, 97, 122, 32, 
	83, 65, 90, 97, 122, 32, 69, 65, 
	90, 97, 122, 32, 65, 90, 97, 122, 
	32, 65, 66, 90, 97, 122, 32, 89, 
	65, 90, 97, 122, 32, 65, 90, 97, 
	122, 32, 69, 65, 90, 97, 122, 32, 
	84, 65, 90, 97, 122, 32, 85, 65, 
	90, 97, 122, 32, 80, 65, 90, 97, 
	122, 32, 65, 90, 97, 122, 32, 69, 
	65, 90, 97, 122, 32, 65, 66, 90, 
	97, 122, 32, 82, 65, 90, 97, 122, 
	32, 68, 65, 90, 97, 122, 32, 79, 
	65, 90, 97, 122, 32, 87, 65, 90, 
	97, 122, 32, 78, 65, 90, 97, 122, 
	32, 65, 90, 97, 122, 0
};

static const char _rtsp_request_line_single_lengths[] = {
	0, 5, 1, 1, 1, 1, 2, 1, 
	2, 1, 2, 1, 2, 2, 2, 2, 
	2, 2, 2, 1, 2, 2, 2, 2, 
	2, 2, 1, 3, 2, 2, 2, 1, 
	2, 2, 1, 2, 2, 2, 2, 1, 
	2, 2, 2, 2, 2, 2, 2, 1, 
	0, 0
};

static const char _rtsp_request_line_range_lengths[] = {
	0, 2, 2, 1, 5, 1, 5, 3, 
	1, 3, 1, 0, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	0, 0
};

static const short _rtsp_request_line_index_offsets[] = {
	0, 0, 8, 12, 15, 22, 25, 33, 
	38, 42, 47, 51, 53, 58, 63, 68, 
	73, 78, 83, 88, 92, 97, 102, 107, 
	112, 117, 122, 126, 132, 137, 142, 147, 
	151, 156, 161, 165, 170, 175, 180, 185, 
	189, 194, 199, 204, 209, 214, 219, 224, 
	228, 229
};

static const char _rtsp_request_line_indicies[] = {
	2, 3, 4, 5, 6, 0, 0, 1, 
	7, 8, 8, 1, 9, 10, 1, 11, 
	12, 13, 12, 13, 12, 1, 11, 12, 
	1, 11, 14, 12, 15, 12, 15, 12, 
	1, 11, 12, 16, 12, 1, 11, 17, 
	12, 1, 11, 12, 18, 12, 1, 19, 
	11, 12, 1, 20, 1, 7, 21, 8, 
	8, 1, 7, 22, 8, 8, 1, 7, 
	23, 8, 8, 1, 7, 24, 8, 8, 
	1, 7, 25, 8, 8, 1, 7, 26, 
	8, 8, 1, 7, 27, 8, 8, 1, 
	28, 8, 8, 1, 7, 29, 8, 8, 
	1, 7, 30, 8, 8, 1, 7, 31, 
	8, 8, 1, 7, 32, 8, 8, 1, 
	7, 33, 8, 8, 1, 7, 34, 8, 
	8, 1, 35, 8, 8, 1, 7, 36, 
	37, 8, 8, 1, 7, 38, 8, 8, 
	1, 7, 39, 8, 8, 1, 7, 40, 
	8, 8, 1, 41, 8, 8, 1, 7, 
	42, 8, 8, 1, 7, 43, 8, 8, 
	1, 44, 8, 8, 1, 7, 45, 8, 
	8, 1, 7, 46, 8, 8, 1, 7, 
	47, 8, 8, 1, 7, 48, 8, 8, 
	1, 49, 8, 8, 1, 7, 50, 8, 
	8, 1, 7, 51, 8, 8, 1, 7, 
	52, 8, 8, 1, 7, 53, 8, 8, 
	1, 7, 54, 8, 8, 1, 7, 55, 
	8, 8, 1, 7, 56, 8, 8, 1, 
	57, 8, 8, 1, 58, 59, 0
};

static const char _rtsp_request_line_trans_targs[] = {
	2, 0, 12, 20, 27, 35, 40, 3, 
	2, 4, 5, 4, 5, 6, 7, 6, 
	8, 9, 10, 11, 48, 13, 14, 15, 
	16, 17, 18, 19, 3, 21, 22, 23, 
	24, 25, 26, 3, 28, 32, 29, 30, 
	31, 3, 33, 34, 3, 36, 37, 38, 
	39, 3, 41, 42, 43, 44, 45, 46, 
	47, 3, 49, 49
};

static const char _rtsp_request_line_trans_actions[] = {
	1, 0, 1, 1, 1, 1, 1, 3, 
	0, 11, 1, 7, 0, 1, 0, 0, 
	0, 0, 0, 5, 0, 0, 0, 0, 
	0, 0, 0, 0, 14, 0, 0, 0, 
	0, 0, 0, 17, 0, 0, 0, 0, 
	0, 20, 0, 0, 23, 0, 0, 0, 
	0, 26, 0, 0, 0, 0, 0, 0, 
	0, 29, 9, 0
};

static const char _rtsp_request_line_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	9, 0
};

static const int rtsp_request_line_start = 1;
static const int rtsp_request_line_first_final = 48;

static const int rtsp_request_line_en_main = 1;


#line 196 "network/ragel_request_line.c"
	{
	cs = rtsp_request_line_start;
	}

#line 201 "network/ragel_request_line.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _rtsp_request_line_trans_keys + _rtsp_request_line_key_offsets[cs];
	_trans = _rtsp_request_line_index_offsets[cs];

	_klen = _rtsp_request_line_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _rtsp_request_line_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _rtsp_request_line_indicies[_trans];
	cs = _rtsp_request_line_trans_targs[_trans];

	if ( _rtsp_request_line_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _rtsp_request_line_actions + _rtsp_request_line_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 40 "network/ragel_request_line.rl"
	{
            s = p;
        }
	break;
	case 1:
#line 44 "network/ragel_request_line.rl"
	{
            req->method = g_strndup(s, p-s);
        }
	break;
	case 2:
#line 49 "network/ragel_request_line.rl"
	{ req->method_id = RTSP_ID_DESCRIBE; }
	break;
	case 3:
#line 50 "network/ragel_request_line.rl"
	{ req->method_id = RTSP_ID_OPTIONS; }
	break;
	case 4:
#line 51 "network/ragel_request_line.rl"
	{ req->method_id = RTSP_ID_PAUSE; }
	break;
	case 5:
#line 52 "network/ragel_request_line.rl"
	{ req->method_id = RTSP_ID_PLAY; }
	break;
	case 6:
#line 53 "network/ragel_request_line.rl"
	{ req->method_id = RTSP_ID_SETUP; }
	break;
	case 7:
#line 54 "network/ragel_request_line.rl"
	{ req->method_id = RTSP_ID_TEARDOWN; }
	break;
	case 8:
#line 59 "network/ragel_request_line.rl"
	{
            req->version = g_strndup(s, p-s);
        }
	break;
	case 9:
#line 65 "network/ragel_request_line.rl"
	{
            req->object = g_strndup(s, p-s);
        }
	break;
	case 10:
#line 75 "network/ragel_request_line.rl"
	{
            return p-msg;
        }
	break;
#line 329 "network/ragel_request_line.c"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _rtsp_request_line_actions + _rtsp_request_line_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 10:
#line 75 "network/ragel_request_line.rl"
	{
            return p-msg;
        }
	break;
#line 351 "network/ragel_request_line.c"
		}
	}
	}

	_out: {}
	}
#line 84 "network/ragel_request_line.rl"


    if ( cs < rtsp_request_line_first_final )
        return 0;

    cs = rtsp_request_line_en_main; // Kill a warning

    return p-msg;
}
#endif
