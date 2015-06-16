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
#include <string>
#include <map>
#include <string.h>
#include <stdio.h>

#include "feng_utils.h"
#include "fnc_log.h"

#include "feng.h"

#include "media/demuxer.h"
#include "media/demuxer_module.h"
#include "network/rtp.h"
#include "network/rtsp.h"

#include "stream_switch.h"

#include <string>
#include <sstream>

#define DEMUXER_STSW_METADATA_TIMEOUT 5000



#ifdef __cplusplus
extern "C" {
#endif

void demuxer_stsw_global_init(void);
void demuxer_stsw_global_uninit(void);

#ifdef __cplusplus
}
#endif


///////////////////////////////////////////////////
// DemuxerStreamSink prototype

class DemuxerSinkListener:public stream_switch::SinkListener{
  
public:
    DemuxerSinkListener(std::string stream_name, Resource * resource);
    virtual ~DemuxerSinkListener();


    virtual void OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size);    
                                  
    virtual void OnMetadataMismatch(uint32_t mismatch_ssrc);    

       
private: 
    std::string stream_name_;    
    Resource * resource_;
        
};



typedef struct stsw_priv_type{
    stream_switch::StreamSink * sink;
    DemuxerSinkListener *listener;

    int stream_type;
  
    //for live stream time scaler
    /** Real-time timestamp when to start the playback */
    double playback_time;
    int has_sync;
    double delta_time;
    
    
} stsw_priv_type;


///////////////////////////////////////////////////
// DemuxerStreamSink Implementation
DemuxerSinkListener::DemuxerSinkListener(std::string stream_name, Resource * resource)
:stream_name_(stream_name), resource_(resource)
{
    
}

DemuxerSinkListener::~DemuxerSinkListener()
{
    
}

void DemuxerSinkListener::OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size)
{
    using namespace stream_switch;
    TrackList tr_it;
    int ret;


    //sanity check
    if(resource_ == NULL){
        return;
    }    
    if(resource_->info->media_source != MS_live){
        //if not live stream, just drop the live frame
        return;
    }
    
    g_mutex_lock(resource_->lock);
    if(resource_->eor){
        fnc_log(FNC_LOG_DEBUG,
                 "[stsw] The resource is already EOR, drop the frame\n");  
        g_mutex_unlock(resource_->lock); 
        return;        
    }

    stsw_priv_type *priv = (stsw_priv_type *)resource_->private_data;
    int streamType = priv->stream_type;   
    
    double frame_time = (double)frame_info.timestamp.tv_sec + 
                        ((double)frame_info.timestamp.tv_usec) / 1000000.0;
    double pts = resource_->timescaler(resource_, frame_time);
    if(pts < 0){
        //pts invalid, just drop this frame
        fnc_log(FNC_LOG_DEBUG,
                 "[stsw] PTS %f, after scale (original time is %f) is invalid, drop the frame\n",
                     pts , frame_time);  
        g_mutex_unlock(resource_->lock); 
        return;
    }

    for (tr_it = g_list_first(resource_->tracks);
         tr_it !=NULL;
         tr_it = g_list_next(tr_it)) {
         Track *tr = (Track*)tr_it->data;
        if (frame_info.sub_stream_index == tr->info->id || 
            streamType != RAW_STREAM) {
            if(streamType != RAW_STREAM) {
                tr->info->id = frame_info.sub_stream_index;
            }
            fnc_log(FNC_LOG_VERBOSE, "[stsw] Parsing track %s",
                    tr->info->name);  
            tr->properties.dts = pts;
            tr->properties.pts = pts;
            fnc_log(FNC_LOG_VERBOSE,
                    "[stsw] delivery & presentation timestamp %f",
                     pts);  
            tr->properties.frame_duration = 0;
            switch(frame_info.frame_type){
            case MEDIA_FRAME_TYPE_KEY_FRAME:
                tr->properties.frame_type = FT_KEY_FRAME;
                break;
            case MEDIA_FRAME_TYPE_DATA_FRAME:
                tr->properties.frame_type = FT_DATA_FRAME;
                break;
            case MEDIA_FRAME_TYPE_PARAM_FRAME:
                tr->properties.frame_type = FT_PARAM_FRAME;
                break;
            default:
                tr->properties.frame_type = FT_UNKONW;
                break;
            }
            
            if(bq_producer_queue_length(tr->producer) >= 
                (unsigned)resource_->srv->srvconf.buffered_frames){
                fnc_log(FNC_LOG_DEBUG,
                 "[stsw] Buffer queue for this track is over the limit (%d), "
                 "drop the frame\n",
                     resource_->srv->srvconf.buffered_frames);   
                break;                
            }
            
            //pass the frame to the codec parser
            ret = tr->parser->parse(tr, (uint8_t*)frame_data, frame_size);
            if(ret){
                fnc_log(FNC_LOG_DEBUG,
                 "[stsw] parser failed to parse the frame\n");                   
            }
            break;
        }//if (frame_info.sub_stream_index == tr->info->id || 
        
    }//for (tr_it = g_list_first(resource_->tracks);
    
    g_mutex_unlock(resource_->lock); 
}
                                  
void DemuxerSinkListener::OnMetadataMismatch(uint32_t mismatch_ssrc)
{
    if(resource_ != NULL){
        //sanity check
        if(resource_->info->media_source != MS_live){
            //if not live stream, just drop the live frame
            return;
        }
        g_mutex_lock(resource_->lock);
        resource_->eor = TRUE;
        g_mutex_unlock(resource_->lock); 
    }
}



///////////////////////////////////////////////////
// demuxer info and implemenation

void demuxer_stsw_global_init(void)
{
    stream_switch::GlobalInit(false);
}


void demuxer_stsw_global_uninit(void)
{
    stream_switch::GlobalUninit();
}

typedef std::map<std::string, std::string> UrlParamMap;


static const DemuxerInfo info = {
	"StreamSwitch Sink Demuxer",
	"stsw",
	"OpenSight Studio(www.opensight.cn)",
	"Work as a StreamSwitch sink to acquire and analyze "
    "the stream from one source" ,
	"", 
    1
};



static int stsw_probe(const char *filename)
{
    if(filename == NULL){
        return RESOURCE_DAMAGED;
    }
    if(strstr(filename, "stsw/stream/") != NULL){
        return RESOURCE_OK;
    }else{
        return RESOURCE_NOT_FOUND;
    }
    
}

static int stsw_url_parse(char * mrl, std::string * stream_name, 
                     UrlParamMap * params)
{
    if(mrl == NULL){
        return RESOURCE_NOT_FOUND;
    }
    /* get stream_name */
    char *stream_name_begin, *param_begin;
    size_t stream_name_len, param_len;
    if((stream_name_begin = strstr(mrl, "stsw/stream/")) == NULL){
        return RESOURCE_NOT_FOUND;
    }
    stream_name_begin += strlen("stsw/stream/");
    stream_name_len = strlen(mrl) - 
        (size_t)(stream_name_begin - mrl);
        
    param_begin = strchr(stream_name_begin, '?');
    if(param_begin == NULL){
        param_len = 0;
    }else{
        param_begin++;
        stream_name_len = (size_t)(param_begin - stream_name_begin - 1);
        param_len = strlen(mrl) - 
            (size_t)(param_begin - mrl);
    }
    
    
    if(stream_name != NULL){
        stream_name->assign(stream_name_begin, stream_name_len);
    }

    
    if(params != NULL){
        params->clear();
        
        char* next_param = NULL;
        std::string attr;
        while (NULL != param_begin){

            next_param = strstr(param_begin, "&");

            if ( next_param != NULL )
            {
                attr.assign(param_begin, (size_t)(next_param - param_begin));
                next_param++;                              //加1，跳过‘&’，好查找下个属性
                param_begin = next_param;
            }else{
                //最后一个属性
                attr.assign(param_begin);
                param_begin = NULL;                                     //确保循环退出
            }
            
            size_t sep = attr.find('=');
            if(sep !=  std::string::npos){
                (*params)[attr.substr(0, sep)] = 
                    attr.substr(sep + 1);
            }

        }/*while (NULL != next_param)*/
    }/*if(params != NULL){*/

    return RESOURCE_OK;
}

static std::string int2str(int int_value)
{
    std::stringstream stream;
    stream<<int_value;
    return stream.str();
}

static double stsw_timescaler (Resource *r, double res_time) {

    stsw_priv_type * priv = (stsw_priv_type *)r->private_data;
    
    if(r->info->media_source == MS_live){
        
        if(priv->has_sync == 0){
            double now = ev_now(r->srv->loop);
            if(now < priv->playback_time){
                now = priv->playback_time;
            }
            priv->delta_time = (now - priv->playback_time) - res_time;
            priv->has_sync = 1;
            fnc_log(FNC_LOG_DEBUG, "[stsw] time sync- org %f, now %f, pts %f\n",
                res_time, now, res_time + priv->delta_time);             
        }
        double scale_time = res_time + priv->delta_time;
        if(scale_time < 0 && scale_time > -0.001){
            scale_time = 0.0;  //allow in 1 ms error
        }
        
        return scale_time;
    }else{
        //for replay stream, just use the original frame time
        return res_time;
    }
    
    return res_time;
}

static int stsw_init(Resource * r)
{
    using namespace stream_switch;    
    std::string stream_name; 
    UrlParamMap params;
    int ret;
    MediaProperties props;
    Track *track = NULL;
    TrackInfo trackinfo;
    int pt = 96;
    UrlParamMap::iterator it;
    bool remote = false;
    int remote_port = 0;
    std::string remote_host;
    StreamClientInfo client_info;
    pid_t pid;
    std::string err_info;
    StreamMetadata metadata;
    stsw_priv_type *priv;
    SubStreamMetadataVector::iterator meta_it;
    
    /* get stream_name */
    ret = stsw_url_parse(r->info->mrl, &stream_name, &params);
    if(ret){
        fnc_log(FNC_LOG_ERR, "[stsw] Cannot Parse %s", r->info->mrl);
        return ret;        
    }
    //fnc_log(FNC_LOG_VERBOSE, "[stsw] Init stream name: %s", stream_name->c_str());    
    
    it = params.find(std::string("host"));
    if(it != params.end()){
        remote = true;
        remote_port = atoi(params["port"].c_str());
        remote_host = it->second;
    }
    
    memset(&trackinfo, 0, sizeof(TrackInfo));
    
    //r->info->seekable = 0;
    //r->info->media_source = MS_live;
    //r->info->duration = HUGE_VAL;
    
    /* init StreamSwitch sink */
    priv = new stsw_priv_type();
    priv->sink = new StreamSink();
    priv->listener = new DemuxerSinkListener(stream_name, r);
    priv->stream_type = r->srv->srvconf.default_stream_type;
    it = params.find(std::string("stream_type"));
    if(it != params.end()){
        if(it->second == "raw"){
            priv->stream_type = RAW_STREAM;
        }else if(it->second == "mp2p"){
            priv->stream_type = MP2P_STREAM;
        }
    }
    
    client_info.client_protocol = "rtsp";
    pid = getpid() ;
    client_info.client_token = int2str(pid % 0xffffff);  
    client_info.client_text = "RTSP Client";
    if(current_client != NULL){
        client_info.client_ip = current_client->host;
        client_info.client_port = current_client->port;        
    }
    if(remote){
        ret = priv->sink->InitRemote(remote_host,remote_port, client_info, 
                         STSW_SUBSCRIBE_SOCKET_HWM, priv->listener, 
                         r->srv->srvconf.stsw_debug_flags, 
                         &err_info);
    }else{
         ret = priv->sink->InitLocal(stream_name, client_info, 
                         STSW_SUBSCRIBE_SOCKET_HWM, priv->listener, 
                         r->srv->srvconf.stsw_debug_flags, 
                         &err_info);   
    }
    if(ret){
        fnc_log(FNC_LOG_ERR, "[stsw] Init Stream Sink failed (%d): %s",
                ret, err_info.c_str());  
        ret = RESOURCE_DAMAGED;
        goto error_1;
    }
       
      
    
    /* get meta data */ 
    ret = priv->sink->UpdateStreamMetaData(DEMUXER_STSW_METADATA_TIMEOUT, 
                                           &metadata, 
                                           &err_info);
    if(ret){
        fnc_log(FNC_LOG_ERR, "[stsw] Get remote metadata failed (%d): %s",
                ret, err_info.c_str());  
        ret = RESOURCE_DAMAGED;
        goto error_1;
    }    
    
    /* check if live stream or replay stream,  
     * only support live stream now
     */
    if(metadata.play_type == STREAM_PLAY_TYPE_REPLAY){
        
        fnc_log(FNC_LOG_ERR, "[stsw] Replay stream is not supported yet\n");  
        ret = RESOURCE_DAMAGED;
        goto error_1;
    }else{
        r->model = MM_PUSH;
        r->info->seekable = FALSE;
        r->info->duration = HUGE_VAL;
        r->info->media_source = MS_live;
        
    }
    
    /* configure the resource and tracks */
    strncpy(trackinfo.title, "StreamSwitch", 80);
    strncpy(trackinfo.author, "OpenSight team", 80);
    
    
    if(priv->stream_type != RAW_STREAM) {
        int videoAvailable = -1;
        memset(&props, 0, sizeof(MediaProperties));
        trackinfo.id = 0;          
        snprintf(trackinfo.name,sizeof(trackinfo.name),"0");
        props.clock_rate = 90000; //Default   

        props.media_source = r->info->media_source;
 
        props.extradata = NULL;
        props.extradata_len = 0;
        strncpy(props.encoding_name, "MP2P", 11);
        props.codec_id = 0;
        props.codec_sub_id = 0;
        props.payload_type = 96;
        props.media_type   = MP_video;        
        
        for(meta_it=metadata.sub_streams.begin();
            meta_it!=metadata.sub_streams.end();
            meta_it++){
                

            switch(meta_it->media_type){
                case SUB_STREAM_MEIDA_TYPE_AUDIO:
                    break;
                case SUB_STREAM_MEIDA_TYPE_VIDEO:
                    videoAvailable = meta_it->sub_stream_index;
                    props.bit_rate     = 0;
                    props.frame_rate   = meta_it->media_param.video.fps;
                    if(props.frame_rate){
                        props.frame_duration     = (double)1 / props.frame_rate;
                    
                    }                    
                    break;
                case SUB_STREAM_MEIDA_TYPE_TEXT:
                case SUB_STREAM_MEIDA_TYPE_PRIVATE:
                default:
                    fnc_log(FNC_LOG_DEBUG, "[stsw] codec type unsupported");
                    break;
            }
        }
        if(videoAvailable != -1) {

            if (!(track = add_track(r, &trackinfo, &props))){
                goto err_alloc;
            }
        }
    }else{

    
        for(meta_it=metadata.sub_streams.begin();
            meta_it!=metadata.sub_streams.end();
            meta_it++){
            
            trackinfo.id = meta_it->sub_stream_index;
            //fnc_log(FNC_LOG_DEBUG, "[stsw] trackinfo.name = %d", meta_it->sub_stream_index);  
            snprintf(trackinfo.name, sizeof(trackinfo.name), "%d", meta_it->sub_stream_index);
            
            memset(&props, 0, sizeof(MediaProperties));
            props.clock_rate = 90000; //Default
            props.extradata = (uint8_t*)((meta_it->extra_data.size() > 0)?meta_it->extra_data.data():NULL);
            props.extradata_len = meta_it->extra_data.size();
            strncpy(props.encoding_name, meta_it->codec_name.c_str(), 10);
            //props.codec_id = codec->codec_id;
            //props.codec_sub_id = codec->codec_id;
            props.payload_type = 96;
            props.frame_type = FT_UNKONW;
            if (props.payload_type == 96){                    
                props.payload_type = pt++;
            }else if (props.payload_type > 96) {
                if(pt <= props.payload_type ) {
                    pt = props.payload_type + 1;
                }
            }

            props.media_source = r->info->media_source;


            fnc_log(FNC_LOG_DEBUG, "[stsw] Parsing Stream %s",
                            props.encoding_name);

            switch(meta_it->media_type){
            case SUB_STREAM_MEIDA_TYPE_AUDIO:
                props.media_type     = MP_audio;
                // Some properties, add more?
                props.audio_channels = meta_it->media_param.audio.channels;
                        // Make props an int...
                props.sample_rate    = meta_it->media_param.audio.samples_per_second;
                if(props.sample_rate ){
                    props.frame_duration = (double)meta_it->media_param.audio.sampele_per_frame * 
                                           ((double)1 / props.sample_rate );
                    
                }
                props.bit_per_sample   = meta_it->media_param.audio.bits_per_sample;
                props.bit_rate = props.bit_per_sample * props.sample_rate;
                        
                if (!(track = add_track(r, &trackinfo, &props)))
                    goto error_1;
                break;
            case SUB_STREAM_MEIDA_TYPE_VIDEO:
                props.media_type   = MP_video;
                props.bit_rate     = 0;
                props.frame_rate   = meta_it->media_param.video.fps;
                if(props.frame_rate){
                    props.frame_duration     = (double)1 / props.frame_rate;
                    
                }

                // addtrack must init the parser, the parser may need the
                // extradata
                if (!(track = add_track(r, &trackinfo, &props)))
                    goto error_1;
                break;
            case SUB_STREAM_MEIDA_TYPE_TEXT:
            case SUB_STREAM_MEIDA_TYPE_PRIVATE:
            default:
                fnc_log(FNC_LOG_DEBUG, "[stsw] codec type unsupported");
                break;
            }
                    
        }//for(it=metadata.sub_streams.begin();
        
    }//if(priv->stream_type != RAW_STREAM) 
    
    if(track == NULL){
        fnc_log(FNC_LOG_ERR, "[stsw] No tracks for the resource %s\n", 
                r->info->mrl);
        ret = RESOURCE_DAMAGED;
        goto error_1;
    }
    
    
    if(r->info->media_source == MS_live){
        fnc_log(FNC_LOG_DEBUG, "[stsw] init live stream successfully\n");
    }else{
        fnc_log(FNC_LOG_DEBUG, "[stsw] init replay stream with duration %f", r->info->duration);
        
    }
    
    r->timescaler = stsw_timescaler;
    r->private_data = priv;
    
    return RESOURCE_OK;
 
error_1:

    if(priv != NULL){
        if(priv->sink != NULL){
            priv->sink->Uninit();
            delete priv->sink;
            priv->sink = NULL;
        }

        if(priv->listener != NULL){
            delete priv->listener;
            priv->listener = NULL;
        }
        delete priv;
        priv = NULL;
    }

    return ret;

 
}

static int stsw_read_packet(Resource * r)
{
    return RESOURCE_EOF;

}

static int stsw_seek(Resource * r, double time_sec)
{
    return RESOURCE_OK;
   
}

static void stsw_uninit(gpointer rgen)
{
    using namespace stream_switch;  
    Resource *r = (Resource *)rgen;
    stsw_priv_type * priv = (stsw_priv_type *)r->private_data;

    
    /* uninit the sink */
    if(priv != NULL){
        if(priv->sink != NULL){
            priv->sink->Uninit();
            delete priv->sink;
            priv->sink = NULL;
        }

        if(priv->listener != NULL){
            delete priv->listener;
            priv->listener = NULL;
        }
        delete priv;
        priv = NULL;
    } 
       
  
}


static int stsw_start(Resource * r)
{
    int ret;
    std::string err_info;
    /* start the sink */
    stsw_priv_type * priv = (stsw_priv_type *)r->private_data;    
    if(priv != NULL){
        RTSP_session *session = (RTSP_session *)r->rtsp_sess;
        RTSP_Range *range = (RTSP_Range *)g_queue_peek_head(session->play_requests);
        if(range == NULL){
            //this situation should not happen
            return RESOURCE_DAMAGED;
        }
        

        
        if(priv->sink != NULL && (!priv->sink->IsStarted())){            
            
            //for live stream, resync the time
            priv->playback_time = range->playback_time;
            priv->delta_time = 0;
            priv->has_sync = 0;  

        
            ret = priv->sink->Start(&err_info);
            if(ret){
                fnc_log(FNC_LOG_ERR, "[stsw] Fail to start the StreamSwitch Sink(%d): %s\n", 
                    ret, err_info.c_str());
                return RESOURCE_DAMAGED;                
            }
            
            //request the key frame now
            ret = priv->sink->KeyFrame(DEMUXER_STSW_METADATA_TIMEOUT, 
                                       &err_info);
            if(ret){
                fnc_log(FNC_LOG_ERR, "[stsw] Fail to request the key frame(%d): %s\n", 
                    ret, err_info.c_str());
                priv->sink->Stop();
                return RESOURCE_DAMAGED;                
            }
                                       

            
        }
        
        return RESOURCE_OK;
    }
    
    return RESOURCE_OK;
}

static void stsw_pause(Resource * r)
{
    /* stop the sink */
    if(r->info->media_source == MS_live){
        //for live stream, stop the sink
        stsw_priv_type * priv = (stsw_priv_type *)r->private_data;    
        if(priv != NULL){
            if(priv->sink != NULL ){
                g_mutex_unlock(r->lock);
                priv->sink->Stop();
                g_mutex_lock(r->lock);
            }      
  
        }//if(priv != NULL)        
    }else{
        // for replay stream, don't stop sink to keep the sink send out
        // heartbeat to the source
        
    }
}

#ifdef __cplusplus
extern "C" {
#endif

FNC_LIB_DEMUXER(stsw);



#ifdef __cplusplus
}
#endif
