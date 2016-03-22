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

#include "libavutil/avassert.h"
#include "libavutil/mathematics.h"
#include "libavutil/parseutils.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include "libavutil/fifo.h"

#include "libavformat/avformat.h"
    
#include "../stsw_cached_segment.h"


static int dummy_init(CachedSegmentContext *cseg)
{
    fprintf(stderr, "dummy_init: URL %s is initialized\n", 
           cseg->filename);
    return 0;
}

static int dummy_write_segment(CachedSegmentContext *cseg, CachedSegment *segment)
{
    
    //sleep(60);
    //fprintf(stderr, "writer pause\n");
    //return 1;
    fprintf(stderr, "Segment(size:%d, start_ts:%.3f, duration:%.3f, pos:%lld, sequence:%lld) is written\n", 
           segment->size, 
           segment->start_ts, segment->duration, 
           segment->pos, segment->sequence); 
    return 0;
}

static void dummy_uninit(CachedSegmentContext *cseg)
{
    fprintf(stderr, "dummy_uninit: URL %s is un-initialized\n", 
           cseg->filename);    
}
CachedSegmentWriter cseg_dummy_writer = {
    .name           = "dummy_writer",
    .long_name      = "OpenSight dummy segment writer", 
    .protos         = "dummy", 
    .init           = dummy_init, 
    .write_segment  = dummy_write_segment, 
    .uninit         = dummy_uninit,
};

