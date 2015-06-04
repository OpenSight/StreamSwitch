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

#ifndef FN_UTILS_H
#define FN_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/types.h>
#include <math.h>
#include <string.h>




/*! autodescriptive error values */
#define ERR_NOERROR              0
#define ERR_GENERIC             -1
#define ERR_NOT_FOUND           -2
#define ERR_PARSE               -3
#define ERR_ALLOC               -4
#define ERR_INPUT_PARAM         -5
#define ERR_NOT_SD              -6  // XXX check it
#define ERR_UNSUPPORTED_PT      -7  /// Unsupported Payload Type
#define ERR_EOF                 -8
#define ERR_FATAL               -9
#define ERR_CONNECTION_CLOSE    -10

/**
 * Returns the current time in seconds
 */

static inline double gettimeinseconds(void) {
    struct timeval tmp;
    gettimeofday(&tmp, NULL);
    return (double)tmp.tv_sec + (double)tmp.tv_usec * .000001;
}


#ifdef __cplusplus
}
#endif //

#endif // FN_UTILS_H
