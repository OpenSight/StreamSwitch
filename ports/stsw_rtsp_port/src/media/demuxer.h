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

#ifndef FN_DEMUXER_H
#define FN_DEMUXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <stdint.h>
#include "bufferqueue.h"

struct feng;

#define RESOURCE_OK 0
#define RESOURCE_NOT_FOUND -1
#define RESOURCE_DAMAGED -2
#define RESOURCE_TRACK_NOT_FOUND -4
#define RESOURCE_NOT_PARSEABLE -5
#define RESOURCE_EOF -6

#define MAX_TRACKS 20
#define MAX_SEL_TRACKS 5

typedef enum {
    MP_undef = -1,
    MP_audio,
    MP_video,
    MP_application,
    MP_data,
    MP_control
} MediaType;

typedef enum {
    MS_stored=0,
    MS_live
} MediaSource;

typedef enum {
    MM_PULL=0,
    MM_PUSH
} MediaReadModel;

typedef enum {
    FT_UNKONW = 0,
    FT_KEY_FRAME = 1,
    FT_DATA_FRAME = 2,
    FT_PARAM_FRAME = 3,
} FrameType;

//! typedefs that give convenient names to GLists used
typedef GList *TrackList;
typedef GList *SelList;

struct MediaParser;

typedef enum {empty_field, fmtp, rtpmap} sdp_field_type;

typedef struct {
	sdp_field_type type;
	char *field;
} sdp_field;

typedef struct ResourceInfo_s {
    char *mrl;
    char *name;
    char *description;
    char *descrURI;
    char *email;
    char *phone;
    time_t mtime;
    double duration;
    MediaSource media_source;
    char twin[256];
    char multicast[16];
    int port;
    char ttl[4];

    /**
     * @brief Seekable resource flag
     *
     * Right now this is only false when the resource is a live
     * resource (@ref ResourceInfo::media_source set to @ref MS_live)
     * or when the demuxer provides no @ref Demuxer::seek function.
     *
     * In the future this can be extended to be set to false if the
     * user disable seeking in a particular stored (and otherwise
     * seekable) resource.
     */
    gboolean seekable;
    
    

    
} ResourceInfo;

typedef struct Resource {
    GMutex *lock;
    const struct Demuxer *demuxer;
    ResourceInfo *info;
    // Metadata begin

    // Metadata end

    /* Timescale fixer callback function for meta-demuxers */
    double (*timescaler)(struct Resource *, double);
    /* EDL specific data */
    struct Resource *edl;
    /* Multiformat related things */
    TrackList tracks;
    int num_tracks;
    gboolean eor;
    void *private_data; /* Demuxer private data */
    struct feng *srv;

    void * rtsp_sess;
    double lastTimestamp;

    MediaReadModel model;
} Resource;

typedef struct Trackinfo_s {
    char *mrl;
    char name[256];
    int id; // should it more generic?
    int rtp_port;
    //start CC
    char commons_deed[256];
    char rdf_page[256];
    char title[256];
    char author[256];
    //end CC
} TrackInfo;

typedef struct MediaProperties {
    int bit_rate; /*!< average if VBR or -1 is not useful*/
    int payload_type;
    unsigned int clock_rate;
    char encoding_name[11];
    MediaType media_type;
    MediaSource media_source;
    int codec_id; /*!< Codec ID as defined by ffmpeg */
    int codec_sub_id; /*!< Subcodec ID as defined by ffmpeg */
    double pts;             //time is in seconds
    double dts;             //time is in seconds
    double frame_duration;  //time is in seconds
    FrameType frame_type;   //the type of the current frame
    float sample_rate;/*!< SamplingFrequency*/
    float OutputSamplingFrequency;
    int audio_channels;
    int bit_per_sample;/*!< BitDepth*/
    float frame_rate;
    int FlagInterlaced;
    unsigned int PixelWidth;
    unsigned int PixelHeight;
    unsigned int DisplayWidth;
    unsigned int DisplayHeight;
    unsigned int DisplayUnit;
    unsigned int AspectRatio;
    uint8_t *ColorSpace;
    float GammaValue;
    uint8_t *extradata;
    size_t extradata_len;
} MediaProperties;

typedef struct Track {
    TrackInfo *info;
    double start_time;
    struct MediaParser *parser;
    /*feng*/
    BufferQueue_Producer *producer;
    Resource *parent;

    uint32_t packetTotalNum;
    int producer_freed;

    void *private_data; /* private data of media parser */

    GSList *sdp_fields;

    MediaProperties properties;
} Track;

typedef struct {
    /*name of demuxer module*/
    const char *name;
    /* short name (for config strings) (e.g.:"sd") */
    const char *short_name;
    /* author ("Author name & surname <mail>") */
    const char *author;
    /* any additional comments */
    const char *comment;
    /* served file extensions */
    const char *extensions; // coma separated list of extensions (w/o '.')
    
    const int fake_path;   // use a fake pattern path instead of a real filesytem path for mrl
} DemuxerInfo;

typedef struct Demuxer {
    const DemuxerInfo *info;
    int (*probe)(const char *filename);
    int (*init)(Resource *);
    int (*read_packet)(Resource *);
    int (*seek)(Resource *, double time_sec);
    GDestroyNotify uninit;
    int (*start)(Resource *);
    void (*pause)(Resource *);
    //...
} Demuxer;

typedef enum {
    RAW_STREAM = 0,
    PS_STREAM,
    MP2P_STREAM
} StreamType;


// --- functions --- //

Resource *r_open(struct feng *srv, const char *inner_path);

int r_read(Resource *resource);
int r_seek(Resource *resource, double time);

void r_close(Resource *resource);

int r_start(Resource *resource);
void r_pause(Resource *resource);


Track *r_find_track(Resource *, const char *);

// Tracks
Track *add_track(Resource *, TrackInfo *, MediaProperties *);
void free_track(gpointer element, gpointer user_data);
void reset_track(gpointer element, gpointer user_data);

void track_add_sdp_field(Track *track, sdp_field_type type, char *value);




#ifdef __cplusplus
}
#endif


#endif // FN_DEMUXER_H
