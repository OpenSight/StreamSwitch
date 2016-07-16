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

#include "libavutil/avassert.h"
#include "libavutil/mathematics.h"
#include "libavutil/parseutils.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include "libavutil/fifo.h"

#include "libavformat/avformat.h"
    
#include "stsw_cached_segment.h"



#define SEGMENT_IO_BUFFER_SIZE 32768
#define MAX_URL_SIZE 4096

#define MIN(a,b) ((a) > (b) ? (b) : (a))

//////////////////////////
//segment operation

CachedSegment * cached_segment_alloc(uint32_t max_size)
{
    CachedSegment * s;
    s = av_mallocz(sizeof(CachedSegment) + max_size);
    s->next = NULL;
    s->buffer_max_size = max_size;
    s->start_ts = -1.0;
    s->duration = 0.0;
    //s->buffer = av_malloc(max_size);
    s->size = 0;
    s->start_pts = AV_NOPTS_VALUE;
    
/*    
    av_log(NULL, AV_LOG_WARNING, 
           "new segment(size:%d) allocated\n",
           max_size);
*/
    return s;    
}
void cached_segment_free(CachedSegment * segment)
{
    //av_free(segment->buffer);
    av_free(segment);
}

void cached_segment_reset(CachedSegment * segment)
{
    segment->start_ts = -1.0;
    segment->duration = 0.0;
    segment->pos = 0;
    segment->next = NULL;
    segment->sequence = 0;
    segment->size = 0;
    segment->start_pts = AV_NOPTS_VALUE;
}
int write_segment(void *opaque, uint8_t *buf, int buf_size)
{  
    CachedSegment * segment = (CachedSegment *) opaque;
    if((segment->buffer_max_size - segment->size) < buf_size){
        return -1;
    }
    memcpy(segment->buffer + segment->size, buf, buf_size);
    segment->size += buf_size;

    return buf_size;
} 


//////////////////////////
//segment list operation
void init_segment_list(CachedSegmentList * seg_list)
{
    seg_list->seg_num = 0;
    seg_list->first = seg_list->last = NULL;
}

void put_segment_list(CachedSegmentList *seg_list, CachedSegment * segment)
{
    segment->next = NULL;    
    if(seg_list->first == NULL){
        seg_list->first = segment;
    }else{
        seg_list->last->next = segment;
        
    }
    seg_list->last = segment;
    seg_list->seg_num ++;
}

CachedSegment * get_segment_list(CachedSegmentList *seg_list)
{
    CachedSegment * segment;
    if(seg_list->first == NULL){
        return NULL;
    }
    segment = seg_list->first;
    seg_list->first = segment->next;
    segment->next = NULL;
    if(seg_list->first == NULL){
        seg_list->last = NULL;
    }
    seg_list->seg_num --;   
    return segment;
    
}


void free_segment_list(CachedSegmentList *seg_list)
{
    CachedSegment * segment = seg_list->first;

    while(segment != NULL){
        CachedSegment * segment_to_free = segment;
        segment = segment->next;
        cached_segment_free(segment_to_free);
    }
    init_segment_list(seg_list);   
}



/////////////////////////////
//writer operations 
static CachedSegmentWriter * first_writer = NULL;

void register_segment_writer(CachedSegmentWriter * writer)
{
    writer->next = first_writer;
    first_writer = writer;
}
static CachedSegmentWriter *find_segment_writer(char * filename)
{
    char hostname[1024], hoststr[1024], proto[16];
    char auth[1024];
    char path[MAX_URL_SIZE];
    int port, i;
    CachedSegmentWriter * writer;
    

    av_url_split(proto, sizeof(proto), auth, sizeof(auth),
                 hostname, sizeof(hostname), &port,
                 path, sizeof(path), filename);
    if(strlen(proto) == 0){
        //no protocol, means file
        strcpy(proto, "file");
    }
    for(writer = first_writer; writer != NULL; writer = writer->next){
        if(av_match_name(proto, writer->protos)){
            return writer;
        }        
    }
    return NULL;
    
}

////////////////////////////
//cseg format operations

static int ff_check_interrupt(AVIOInterruptCB *cb)
{
    int ret;
    if (cb && cb->callback && (ret = cb->callback(cb->opaque)))
        return ret;
    return 0;
}


static CachedSegment * get_free_segment(CachedSegmentContext *cseg)
{
    CachedSegment * segment = NULL;
    pthread_mutex_lock(&cseg->mutex);
    if(cseg->free_list.seg_num > 0){
        segment = get_segment_list(&(cseg->free_list));
        cached_segment_reset(segment);
    }
    pthread_mutex_unlock(&cseg->mutex);
    if(segment == NULL){
        segment = cached_segment_alloc(cseg->max_seg_size);
    }
    return segment;
}

static void recycle_free_segment(CachedSegmentContext *cseg, CachedSegment * segment)
{
    cached_segment_reset(segment);
    pthread_mutex_lock(&cseg->mutex);        
    put_segment_list(&(cseg->free_list), segment);        
    pthread_mutex_unlock(&cseg->mutex);
}

/* append current segment to the cached segment list */
static int append_cur_segment(AVFormatContext *s)
{
    CachedSegmentContext *cseg = (CachedSegmentContext *)s->priv_data;
    CachedSegment * segment = cseg->cur_segment;
    
    if(segment == NULL){
        //no current segment, just finished
        return 0;
    }
    
    cseg->cur_segment = NULL;
       
    if(segment->start_ts <= 0.0 ||
       segment->duration < 1){
        //segment is invalid
        recycle_free_segment(cseg, segment);
        return 0;
    }
        
    pthread_mutex_lock(&cseg->mutex);
    if(!(cseg->flags & CSEG_FLAG_NONBLOCK)){
        while(cseg->cached_list.seg_num >= cseg->max_nb_segments){
            pthread_mutex_unlock(&cseg->mutex);            
            if (ff_check_interrupt(&s->interrupt_callback)){ 
                recycle_free_segment(cseg, segment);
                return AVERROR_EXIT;  
            }else if(cseg->consumer_exit_code){
                recycle_free_segment(cseg, segment);
                return cseg->consumer_exit_code;
            }
            av_usleep(10000); //wait for 10ms
            pthread_mutex_lock(&cseg->mutex);
        }  
    }//if(!(cseg->flags & CSEG_FLAG_NONBLOCK)){
        
    
    if(cseg->cached_list.seg_num >= cseg->max_nb_segments){ 
        av_log(s, AV_LOG_WARNING, 
               "One Segment(size:%d, start_ts:%f, duration:%f, pos:%lld, sequence:%lld) "
               "is dropped because of slow writer\n", 
                segment->size, 
                segment->start_ts, segment->duration, 
                segment->pos, segment->sequence); 
        cached_segment_reset(segment);
        put_segment_list(&(cseg->free_list), segment);         
    }else{
/*
        av_log(s, AV_LOG_INFO, 
                "One Segment(size:%d, start_ts:%f, duration:%f, pos:%lld, sequence:%lld) "
                "is added to cached list(len:%d)\n", 
                segment->size, 
                segment->start_ts, segment->duration, 
                segment->pos, segment->sequence, 
                cseg->cached_list.seg_num); 
*/
        put_segment_list(&(cseg->cached_list), segment);           
    }
    pthread_cond_signal(&cseg->not_empty); //wakeup comsumer    
    
    pthread_mutex_unlock(&cseg->mutex);
    
    return 0;
}

static void * consumer_routine(void *arg)
{
    CachedSegmentContext *cseg = 
        (CachedSegmentContext *)arg;
    CachedSegment * segment = NULL;
    int ret = 0;
   
    
    pthread_mutex_lock(&cseg->mutex);
    while(cseg->consumer_active){
        int keep_seg_num = 0;         
        
        //try write out all segment in cached list
        while((segment = cseg->cached_list.first) != NULL){            
            ret = 0;
            if(cseg->writer != NULL && cseg->writer->write_segment != NULL){   
                pthread_mutex_unlock(&cseg->mutex);
                //because there is only one comsumer, the first segment is safe to access without lock
                ret = cseg->writer->write_segment(cseg, segment);
                pthread_mutex_lock(&cseg->mutex);
            } 
            if(ret == 0){
                //successful
                
                //remove the segment from cached list
                segment = get_segment_list(&(cseg->cached_list));                
                cached_segment_reset(segment);          
                put_segment_list(&(cseg->free_list), segment);                        
                
            }else if(ret == 1){
                //should keep in fifo
                break;
            }else if(ret < 0){
                //error     
                pthread_mutex_unlock(&cseg->mutex);
                cseg->consumer_exit_code = ret;
                pthread_exit(NULL);     
            }else{
                //not support other ret code, consider error
                pthread_mutex_unlock(&cseg->mutex);
                av_log(NULL, AV_LOG_ERROR,  "[cseg] cannot support the writer return code:%d\n", ret);        
                cseg->consumer_exit_code = AVERROR(EINVAL);
                pthread_exit(NULL); 
            }
        }// while((segment = cseg->cached_list.first) != NULL){
        
        //clean up the expired segments
        keep_seg_num = MIN((uint32_t)ceil(cseg->pre_recoding_time / cseg->time), 
                            cseg->max_nb_segments - 1);    
        while(cseg->cached_list.seg_num > keep_seg_num){
            //remove the segment from cached list
            segment = get_segment_list(&(cseg->cached_list));                
            cached_segment_reset(segment);          
            put_segment_list(&(cseg->free_list), segment);              
        }//while(cseg->cached_list.seg_num > keep_seg_num){
            
        if(cseg->consumer_active){
            pthread_cond_wait(&(cseg->not_empty), &(cseg->mutex)); //wait for next time
        }
        
    }//while(cseg->consumer_active){
    pthread_mutex_unlock(&cseg->mutex);
    
    //flush all the cached segment 
    //because cseg->consumer_active is 0 which means no producer existed now, 
    //we don't need lock any more
    while((segment = get_segment_list(&(cseg->cached_list))) != NULL){
        //call writer's method
        ret = 0;
        if(cseg->writer != NULL && cseg->writer->write_segment != NULL){                    
            ret = cseg->writer->write_segment(cseg, segment);
        }
        cached_segment_reset(segment);          
        put_segment_list(&(cseg->free_list), segment);         
        
        if(ret < 0){
            //error  
            cseg->consumer_exit_code = ret;
            break;                
        }else if(ret == 0){
            //successful
            
        }else if(ret == 1){
            //should keep in fifo  
            break;
        }else{
            cseg->consumer_exit_code = AVERROR(EINVAL);
            break;   
        }
    }
    
    return NULL;    
}



static int cseg_mux_init(AVFormatContext *s)
{
    CachedSegmentContext *cseg = s->priv_data;
    AVFormatContext *oc;
    int i, ret;

    ret = avformat_alloc_output_context2(&cseg->avf, cseg->oformat, NULL, NULL);
    if (ret < 0)
        return ret;
    oc = cseg->avf;

    oc->oformat            = cseg->oformat;
    oc->interrupt_callback = s->interrupt_callback;
    oc->max_delay          = s->max_delay;
    av_dict_copy(&oc->metadata, s->metadata, 0);


    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st;
        AVFormatContext *loc = oc;
        if (!(st = avformat_new_stream(loc, NULL)))
            return AVERROR(ENOMEM);
        avcodec_copy_context(st->codec, s->streams[i]->codec);
        st->sample_aspect_ratio = s->streams[i]->sample_aspect_ratio;
        st->time_base = s->streams[i]->time_base;
    }
    

    return 0;
}


static int cseg_start(AVFormatContext *s)
{
    CachedSegmentContext *cseg = s->priv_data;
    AVFormatContext *oc = cseg->avf;
    AVDictionary *options = NULL;
    char *filename;
    int err = 0;
    AVIOContext *avio_out = NULL;
    CachedSegment * segment = NULL;
    

    segment = get_free_segment(cseg);
    if(!segment){
        err = AVERROR(ENOMEM);
        return err;      
    }
    
    avio_out = avio_alloc_context(cseg->out_buffer, SEGMENT_IO_BUFFER_SIZE,
                                  1, segment, NULL, &write_segment, NULL);
    if (!avio_out) {
        recycle_free_segment(cseg, segment);
        err = AVERROR(ENOMEM);
        return err;
    }

    avio_out->direct = 1; //direct IO to segment
    oc->pb = avio_out;  
    oc->flags |= AVFMT_FLAG_CUSTOM_IO;
    cseg->cur_segment = segment;
    cseg->number++;   
    segment->sequence = cseg->sequence++;

    if (oc->oformat->priv_class && oc->priv_data)
        av_opt_set(oc->priv_data, "mpegts_flags", "resend_headers", 0);

    return 0;
}



static int cseg_write_header(AVFormatContext *s)
{
    CachedSegmentContext *cseg = s->priv_data;
    int ret, i;
    char *p;
    AVDictionary *options = NULL;
    int basename_size;
    CachedSegmentWriter * writer;
    
    pthread_mutex_init(&cseg->mutex, NULL);
    pthread_cond_init(&cseg->not_empty, NULL);
    cseg->sequence       = cseg->start_sequence;
    cseg->recording_time = cseg->time * AV_TIME_BASE;
    cseg->start_pts = AV_NOPTS_VALUE;
    cseg->start_pos = 0;
    cseg->number = 0;
    cseg->consumer_exit_code = 0;

    if (cseg->format_options_str) {
        ret = av_dict_parse_string(&cseg->format_options, cseg->format_options_str, "=", ":", 0);
        if (ret < 0) {
            av_log(s, AV_LOG_ERROR, "Could not parse format options list '%s'\n", cseg->format_options_str);
            av_dict_free(&cseg->format_options);
            goto fail;
        }
    }

    for (i = 0; i < s->nb_streams; i++) {
        cseg->has_video +=
            s->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO;
        cseg->has_subtitle +=
            s->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE;
    }

    if (cseg->has_video > 1)
        av_log(s, AV_LOG_WARNING,
               "More than a single video stream present, "
               "expect issues decoding it.\n");
    if(cseg->has_subtitle){
        av_log(s, AV_LOG_ERROR,
               "Not support subtitle stream\n");    
        ret = AVERROR_PATCHWELCOME;
        goto fail;        
    }
    
    if(cseg->time < 1.0){
        av_log(s, AV_LOG_ERROR,
               "segment time cannot be less than 1.0 second\n");    
        ret = AVERROR_INVALIDDATA;
        goto fail;          
    }

    cseg->oformat = av_guess_format("mpegts", NULL, NULL);
    if (!cseg->oformat) {
        ret = AVERROR_MUXER_NOT_FOUND;
        goto fail;
    }

    cseg->filename = av_strdup(s->filename);
    cseg->out_buffer = av_malloc(SEGMENT_IO_BUFFER_SIZE);
    init_segment_list(&cseg->cached_list);
    init_segment_list(&cseg->free_list);    

    if ((ret = cseg_mux_init(s)) < 0)
        goto fail;

    if ((ret = cseg_start(s)) < 0)
        goto fail;

    av_dict_copy(&options, cseg->format_options, 0);
    ret = avformat_write_header(cseg->avf, &options);
    if (av_dict_count(options)) {
        av_log(s, AV_LOG_ERROR, "Some of provided format options in '%s' are not recognized\n", cseg->format_options_str);
        ret = AVERROR(EINVAL);
        goto fail;
    }
    //av_assert0(s->nb_streams == cseg->avf->nb_streams);
    for (i = 0; i < s->nb_streams; i++) {
        AVStream *inner_st;
        AVStream *outer_st = s->streams[i];
        if (outer_st->codec->codec_type != AVMEDIA_TYPE_SUBTITLE)
            inner_st = cseg->avf->streams[i];
        else {
            /* We have a subtitle stream, when the user does not want one */
            inner_st = NULL;
            continue;
        }
        avpriv_set_pts_info(outer_st, inner_st->pts_wrap_bits, inner_st->time_base.num, inner_st->time_base.den);
    }
    
    //find writer
    cseg->writer = find_segment_writer(cseg->filename);
    if(!cseg->writer){
        av_log(s, AV_LOG_ERROR, "No writer found for url:%s\n", cseg->filename);
        ret = AVERROR_MUXER_NOT_FOUND;
        goto fail;
    }
    if(cseg->writer->init){
        ret = cseg->writer->init(cseg);
        if(ret<0){
            av_log(s, AV_LOG_ERROR, "Writer(%s) init failed for url:%s\n", 
            cseg->writer->name,
            cseg->filename);  
            cseg->writer = NULL;
            goto fail;
        }
    }   
    
    //successful write header, start consumer
    cseg->consumer_active = 1;
    cseg->consumer_exit_code = 0;
    ret = pthread_create(&(cseg->consumer_thread_id), NULL, consumer_routine, cseg);
    if(ret){
        av_log(s, AV_LOG_ERROR, "Start consumer thread failed");
        ret = AVERROR(ret);
        cseg->consumer_active = 0;
        cseg->consumer_thread_id  = 0;
        goto fail;
    }    
    
fail:
    av_dict_free(&options);
    if (ret < 0) {
        if(cseg->writer){
            if(cseg->writer->uninit){
                cseg->writer->uninit(cseg);
            }
            cseg->writer = NULL;
        }
        
        if (cseg->avf){
            AVFormatContext *oc = cseg->avf;
            if(oc->pb){
                av_freep(&(oc->pb));
            }
            avformat_free_context(cseg->avf);
        }
        if(cseg->cur_segment){
            cached_segment_free(cseg->cur_segment);
            cseg->cur_segment = NULL;
        }
        if(cseg->out_buffer != NULL){
            av_freep(&cseg->out_buffer);
        }
        
        av_freep(&cseg->filename);
        
        if(cseg->format_options){
            av_dict_free(&cseg->format_options);            
        }
        pthread_cond_destroy(&cseg->not_empty);
        pthread_mutex_destroy(&cseg->mutex);        
    }
    return ret;
}


static int cseg_ff_write_chained(AVFormatContext *dst, int dst_stream, AVPacket *pkt,
                     AVFormatContext *src, int interleave)
{
    AVPacket local_pkt;
    int ret;

    local_pkt = *pkt;
    local_pkt.stream_index = dst_stream;
    if (pkt->pts != AV_NOPTS_VALUE)
        local_pkt.pts = av_rescale_q(pkt->pts,
                                     src->streams[pkt->stream_index]->time_base,
                                     dst->streams[dst_stream]->time_base);
    if (pkt->dts != AV_NOPTS_VALUE)
        local_pkt.dts = av_rescale_q(pkt->dts,
                                     src->streams[pkt->stream_index]->time_base,
                                     dst->streams[dst_stream]->time_base);
    if (pkt->duration)
        local_pkt.duration = av_rescale_q(pkt->duration,
                                          src->streams[pkt->stream_index]->time_base,
                                          dst->streams[dst_stream]->time_base);
  
    if (interleave) ret = av_interleaved_write_frame(dst, &local_pkt);
    else            ret = av_write_frame(dst, &local_pkt);
    pkt->buf = local_pkt.buf;
    pkt->side_data       = local_pkt.side_data;
    pkt->side_data_elems = local_pkt.side_data_elems;
#if FF_API_DESTRUCT_PACKET

    pkt->destruct = local_pkt.destruct;

#endif
    return ret;
}


static int cseg_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    CachedSegmentContext *cseg = (CachedSegmentContext *)s->priv_data;
    AVFormatContext *oc = cseg->avf;
    AVStream *st = s->streams[pkt->stream_index];
    int64_t end_pts = cseg->recording_time * cseg->number;
    int is_ref_pkt = 1;
    int ret, can_split = 1;
    int stream_index = 0;

    stream_index = pkt->stream_index;
    
    if(cseg->consumer_exit_code){
        //consumer error
/*
        av_log(s, AV_LOG_ERROR, 
               "Consumer Error:%s\n",
               cseg->consumer_err_str[0] == 0?
               "unknown":cseg->consumer_err_str); 
*/
        return cseg->consumer_exit_code;
    }
    
    if(cseg->cur_segment == NULL){
        //no current segment
        return AVERROR_EXIT;
    }

    if (cseg->start_pts == AV_NOPTS_VALUE) {
        cseg->start_pts = pkt->pts;
        if(cseg->start_pts != AV_NOPTS_VALUE){
            //start_pts is ready, check start_ts
            if(cseg->start_ts < 0.0){
                //get current time for start ts
                struct timeval tv;
                gettimeofday(&tv, NULL);
                if(tv.tv_sec < 31536000){   //not valid
                    cseg->start_pts = AV_NOPTS_VALUE;
                    av_log(s, AV_LOG_ERROR, 
                           "gettimeofday error, the timestamp is invalid\n");                     
                    return AVERROR_EXIT;
                }
                cseg->start_ts = (double)tv.tv_sec + ((double)tv.tv_usec) / 1000000.0;
            }//if(cseg->start_ts < 0.0){
        }//if(cseg->start_pts != AV_NOPTS_VALUE){
    }//if (cseg->start_pts == AV_NOPTS_VALUE) {
        
    if(cseg->cur_segment->start_pts == AV_NOPTS_VALUE){
        cseg->cur_segment->start_pts = pkt->pts;
        if(cseg->cur_segment->start_pts != AV_NOPTS_VALUE && 
           cseg->cur_segment->start_ts <= 0.0){
            cseg->cur_segment->start_ts = cseg->start_ts;          
        }        
    }
    
   
    if (cseg->has_video) {
        can_split = st->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
                    pkt->flags & AV_PKT_FLAG_KEY;
        is_ref_pkt = st->codec->codec_type == AVMEDIA_TYPE_VIDEO;
    }
    
    if (pkt->pts == AV_NOPTS_VALUE)
        is_ref_pkt = can_split = 0;

    if (is_ref_pkt){
        cseg->cur_segment->duration = (double)(pkt->pts - cseg->cur_segment->start_pts)
                                   * st->time_base.num / st->time_base.den;
    }

    if (can_split && av_compare_ts(pkt->pts - cseg->start_pts, st->time_base,
                                   end_pts, AV_TIME_BASE_Q) >= 0) {
        av_write_frame(oc, NULL); /* Flush any buffered data */
/*        
        printf("pts:%lld, start_pts:%lld, end_pts:%lld, split_end_pts:%lld\n",
               (long long)pkt->pts, (long long)cseg->start_pts, (long long)cseg->end_pts, (long long)end_pts);
*/
        if (oc->pb) {
            int64_t cur_segment_size = 0;
            
            avio_flush(oc->pb);
            av_freep(&(oc->pb));
                
            cur_segment_size = cseg->cur_segment->size;

            ret = append_cur_segment(s); // lose the control of cseg->cur_segment
            if (ret < 0)
                return ret;                
            cseg->start_pos += cur_segment_size;
        }
        
       
        //init new segment
        ret = cseg_start(s);
        if (ret < 0)
            return ret;
        cseg->cur_segment->start_ts = (double)(pkt->pts - cseg->start_pts)
                                            * st->time_base.num / st->time_base.den + cseg->start_ts;        
        cseg->cur_segment->pos = cseg->start_pos;
        cseg->cur_segment->start_pts = pkt->pts;
        cseg->cur_segment->duration = 0.0;
        
    }//if (can_split && av_compare_ts(pkt->pts - cseg->start_pts, st->time_base,

    ret = cseg_ff_write_chained(oc, stream_index, pkt, s, 0);

    return ret;
}

static int cseg_write_trailer(struct AVFormatContext *s)
{
     
    CachedSegmentContext *cseg = s->priv_data;
    AVFormatContext *oc = cseg->avf;
 
    av_write_trailer(oc);

    if (oc->pb) {
        double seg_start_ts;
        int64_t cur_segment_size = 0;
        int is_cached_list_full = 0;
         
        avio_flush(oc->pb);
        av_freep(&(oc->pb));
        
        pthread_mutex_lock(&cseg->mutex);
        if(cseg->cached_list.seg_num >= cseg->max_nb_segments){
            if(cseg->cur_segment != NULL){
                cached_segment_reset(cseg->cur_segment);
                put_segment_list(&(cseg->free_list), cseg->cur_segment);
                cseg->cur_segment = NULL;                
            }
            pthread_mutex_unlock(&cseg->mutex); 
        }else if(cseg->number <= 1){
            //Jam(2016-07-12): if cseg->number equals 1, 
            // means the current segment is the first segment and has not finished,
            // don't write this single unfinished segment to avoid record fragmentation

            if(cseg->cur_segment != NULL){
                cached_segment_reset(cseg->cur_segment);
                put_segment_list(&(cseg->free_list), cseg->cur_segment);
                cseg->cur_segment = NULL;                
            }
            pthread_mutex_unlock(&cseg->mutex);   
            av_log(s, AV_LOG_ERROR,
                    "drop the current single unfinished segment\n");           

        }else{
            pthread_mutex_unlock(&cseg->mutex);  
            append_cur_segment(s); // lose the control of cseg->cur_segment            
        }
    }//if (oc->pb) {
          
    if(cseg->consumer_thread_id != 0){
        void * res;
        int ret;
        pthread_mutex_lock(&cseg->mutex); 
        cseg->consumer_active = 0;          
        pthread_cond_signal(&cseg->not_empty); //wakeup comsumer
        pthread_mutex_unlock(&cseg->mutex);  
                
        ret = pthread_join(cseg->consumer_thread_id, &res);
        if (ret != 0){
            av_log(s, AV_LOG_ERROR, "stop consumer thread failed\n");
        }
        cseg->consumer_thread_id = 0;
        
    }
    
    if(cseg->writer){
        if(cseg->writer->uninit){
            cseg->writer->uninit(cseg);
        }
        cseg->writer = NULL;
    }    

    avformat_free_context(oc);
    cseg->avf = NULL;

    free_segment_list(&(cseg->cached_list));
    free_segment_list(&(cseg->free_list));

    av_freep(&cseg->filename);
 
    if(cseg->out_buffer != NULL){
        av_freep(&cseg->out_buffer);
    }   
    
    if(cseg->format_options){
        av_dict_free(&cseg->format_options);            
    }    
    pthread_cond_destroy(&cseg->not_empty);
    pthread_mutex_destroy(&cseg->mutex); 
   
    return 0;
}

#define OFFSET(x) offsetof(CachedSegmentContext, x)
#define E AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    {"start_number",  "set first number in the sequence",        OFFSET(start_sequence),AV_OPT_TYPE_INT64,  {.i64 = 0},     0, INT64_MAX, E},
    {"cseg_time",      "set segment length in seconds",           OFFSET(time),    AV_OPT_TYPE_DOUBLE,  {.dbl = 10},     0, FLT_MAX, E},
    {"cseg_list_size", "set maximum number of the cache list",  OFFSET(max_nb_segments),    AV_OPT_TYPE_INT,    {.i64 = 3},     1, INT_MAX, E},
    {"cseg_ts_options","set hls mpegts list of options for the container format used for hls", OFFSET(format_options_str), AV_OPT_TYPE_STRING, {.str = NULL},  0, 0,    E},
    {"cseg_seg_size",  "set maximum segment size in bytes",        OFFSET(max_seg_size),AV_OPT_TYPE_INT,  {.i64 = 10485760},     0, INT_MAX, E},
    {"start_ts",      "set start timestamp (in seconds) for the first segment", OFFSET(start_ts),    AV_OPT_TYPE_DOUBLE,  {.dbl = -1.0},     -1.0, DBL_MAX, E},
    {"cseg_cache_time", "set min cache time in seconds for writer pause", OFFSET(pre_recoding_time),    AV_OPT_TYPE_DOUBLE,  {.dbl = 0},     0, DBL_MAX, E},
    {"use_localtime",          "set filename expansion with strftime at segment creation", OFFSET(use_localtime), AV_OPT_TYPE_INT, {.i64 = 0 }, 0, 1, E },
    {"writer_timeout",     "set timeout (in milliseconds) of writer I/O operations", OFFSET(writer_timeout),     AV_OPT_TYPE_INT, { .i64 = 30000 },         -1, INT_MAX, .flags = E },
    {"cseg_flags",     "set flags affecting cached segement working policy", OFFSET(flags), AV_OPT_TYPE_FLAGS, {.i64 = 0 }, 0, UINT_MAX, E, "flags"},
    {"nonblock",   "never blocking in the write_packet() when the cached list is full, instead, dicard the eariest segment", 0, AV_OPT_TYPE_CONST, {.i64 = CSEG_FLAG_NONBLOCK }, 0, UINT_MAX,   E, "flags"},

    { NULL },
};

static const AVClass cseg_class = {
    .class_name = "cached_segment muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};


AVOutputFormat ff_cached_segment_muxer = {
    .name           = "cseg",
    .long_name      = "OpenSight cached segment muxer",
    .priv_data_size = sizeof(CachedSegmentContext),
    .audio_codec    = AV_CODEC_ID_AAC,
    .video_codec    = AV_CODEC_ID_H264,
    .flags          = AVFMT_NOFILE | AVFMT_ALLOW_FLUSH,
    .write_header   = cseg_write_header,
    .write_packet   = cseg_write_packet,
    .write_trailer  = cseg_write_trailer,
    .priv_class     = &cseg_class,
};

extern CachedSegmentWriter cseg_file_writer;
extern CachedSegmentWriter cseg_dummy_writer;
extern CachedSegmentWriter cseg_ivr_writer;

void register_cseg(void)
{
    
    register_segment_writer(&cseg_file_writer);
    register_segment_writer(&cseg_dummy_writer);
    register_segment_writer(&cseg_ivr_writer);    
    av_register_output_format(&ff_cached_segment_muxer);

}