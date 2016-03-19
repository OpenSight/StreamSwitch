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

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/fifo.h"
#include "libavutil/log.h"
#include "libavformat/avformat.h"

struct CachedSegmentContext;
typedef struct CachedSegmentContext CachedSegmentContext;

typedef struct CachedSegment {
    //uint8_t *buffer;
    int size;
    double start_ts; /* start timestamp, in seconds */
    double duration; /* in seconds */
    int64_t pos;
    int buffer_max_size;   
    int64_t sequence;
    struct CachedSegment *next;
    uint8_t buffer[0];
} CachedSegment;


typedef struct CachedSegmentList {
    uint32_t seg_num;
    struct CachedSegment *first, *last;
} CachedSegmentList;

void init_segment_list(CachedSegmentList * seg_list);
void put_segment_list(CachedSegmentList *seg_list, CachedSegment * segment);
CachedSegment * get_segment_list(CachedSegmentList *seg_list);
void free_segment_list(CachedSegmentList *seg_list);



typedef struct CachedSegmentWriter {
    
    /**
     * the short name of the writer
     */     
    const char *name;

    /**
     * Descriptive name for the writer, meant to be more human-readable
     * than name. You should use the NULL_IF_CONFIG_SMALL() macro
     * to define it.
     */
    const char *long_name;
    
    /**
     * A comma separated list of protocols of the URL for the writer.
     */    
    const char *protos;
    
    struct CachedSegmentWriter * next; //next register writer
    
    //return 0 on success, a negative AVERROR on failure.
    int (*init)(CachedSegmentContext *cseg);
    
    //return 0 on success, 
    //       1 on writer pause for the moment
    //       otherwise, return a negative number for error
    int (*write_segment)(CachedSegmentContext *cseg, CachedSegment *segment);
    
    void (*uninit)(CachedSegmentContext *cseg);
} CachedSegmentWriter;
    

typedef enum CachedSegmentFlags {
    CSEG_FLAG_NONBLOCK = (1 << 0),
} CachedSegmentFlags;




struct CachedSegmentContext {
    const AVClass *context_class;  // Class for private options.
    
    char *filename;
    char *format_options_str;   //mpegts options string
    AVDictionary *format_options; //mpegts options
    
    unsigned number;
    int64_t sequence;
    
    AVOutputFormat *oformat;
    AVFormatContext *avf;
    
    CachedSegment * cur_segment;
    unsigned char * out_buffer;
    
    int64_t start_sequence;
    double start_ts;        //the timestamp for the start_pts, start ts for the whole video
    double time;            // Set by a private option.
    int max_nb_segments;   // Set by a private option.
    uint32_t max_seg_size;      // max size for a segment in bytes, set by a private option
    uint32_t flags;        // enum HLSFlags

    int use_localtime;      ///< flag to expand filename with localtime
    int64_t recording_time;  // segment length in 1/AV_TIME_BASE sec
    int has_video;
    int has_subtitle;
    int64_t start_pts;    // start pts for the whole list
    int64_t end_pts;      // end pts for finished segments, 
                          // i.e. the start pts for the current segment
    double seg_start_ts;  // current segment start ts
    double duration;      // current segment duration computed so far, in seconds
    int64_t start_pos;    // current segment starting position

    double pre_recoding_time;   // at least pre_recoding_time should be kept in cached
                                // when segment persistence is disabbled, 
    
    pthread_t consumer_thread_id;
    volatile int consumer_active;
    volatile int consumer_exit_code;
#define CONSUMER_ERR_STR_LEN 1024
    char consumer_err_str[CONSUMER_ERR_STR_LEN];
    
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    CachedSegmentList cached_list;
    CachedSegmentList free_list;
    
    CachedSegmentWriter *writer;
    void * writer_priv;
    int32_t writer_timeout;
};

extern AVOutputFormat ff_cached_segment_muxer;

void register_segment_writer(CachedSegmentWriter * writer);

void register_cseg(void);

#ifdef __cplusplus
}
#endif