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
 * @brief Handling of RTSP headers
 *
 * This file includes all the utilities needed for handling RTSP
 * headers
 */

#ifndef LIBERIS_HEADERS_H__
#define LIBERIS_HEADERS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

/**
 * @defgroup headers Header handling
 *
 * This group includes all the public definition relating to headers
 * handling (structures, parsers, composers, validators, constants...)
 *
 * @{
 */

/**
 * @defgroup strings Header string constants
 *
 * This group includes string constants for the RTSP headers as
 * defined by RFC 2326 Section 12. More non-standard headers may be
 * used, since it just requires a string constant, but these are the
 * standard headers.
 *
 * It is suggested to use the header constants rather than write the
 * name of the header as a literal. The emitted code is not supposed
 * to change, but by using the constants the compiler can catch
 * possible typos in the header names.
 *
 * @{
 */
static const char eris_hdr_accept[] = "Accept";
static const char eris_hdr_accept_encoding[] = "Accept-Encoding";
static const char eris_hdr_accept_language[] = "Accept-Language";
static const char eris_hdr_allow[] = "Allow";
static const char eris_hdr_authorization[] = "Authorization";
static const char eris_hdr_bandwidth[] = "Bandwidth";
static const char eris_hdr_blocksize[] = "Blocksize";
static const char eris_hdr_cache_control[] = "Cache-Control";
static const char eris_hdr_conference[] = "Conference";
static const char eris_hdr_connection[] = "Connection";
static const char eris_hdr_content_base[] = "Content-Base";
static const char eris_hdr_content_encoding[] = "Content-Encoding";
static const char eris_hdr_content_language[] = "Content-Language";
static const char eris_hdr_content_length[] = "Content-Length";
static const char eris_hdr_content_location[] = "Content-Location";
static const char eris_hdr_content_type[] = "Content-Type";
static const char eris_hdr_cseq[] = "CSeq";
static const char eris_hdr_date[] = "Date";
static const char eris_hdr_expires[] = "Expires";
static const char eris_hdr_from[] = "From";
static const char eris_hdr_if_modified_since[] = "If-Modified-Since";
static const char eris_hdr_last_modified[] = "Last-Modified";
static const char eris_hdr_proxy_authenticate[] = "Proxy-Authenticate";
static const char eris_hdr_proxy_require[] = "Proxy-Require";
static const char eris_hdr_public[] = "Public";
static const char eris_hdr_range[] = "Range";
static const char eris_hdr_referer[] = "Referer";
static const char eris_hdr_require[] = "Require";
static const char eris_hdr_retry_after[] = "Retry-After";
static const char eris_hdr_rtp_info[] = "RTP-Info";
static const char eris_hdr_scale[] = "Scale";
static const char eris_hdr_session[] = "Session";
static const char eris_hdr_server[] = "Server";
static const char eris_hdr_speed[] = "Speed";
static const char eris_hdr_timestamp[] = "Timestamp";
static const char eris_hdr_transport[] = "Transport";
static const char eris_hdr_unsupported[] = "Unsupported";
static const char eris_hdr_user_agent[] = "User-Agent";
static const char eris_hdr_via[] = "Via";
static const char eris_hdr_www_authenticate[] = "WWW-Authenticate";
/**
 * @}
 */

size_t eris_parse_headers(const char *hdrs_string, size_t len, GHashTable **table);
void eris_flatten_headers(GHashTable *hdrs, GString *str);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
#endif
