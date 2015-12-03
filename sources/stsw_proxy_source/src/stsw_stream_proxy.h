/**
 * This file is part of stsw_proxy_source, which belongs to StreamSwitch
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
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/**
 * stsw_stream_proxy.h
 *      the header file of StreamProxySource class. 
 * 
 * author: OpenSight Team
 * date: 2015-6-23
**/ 

#include <stream_switch.h>
#include <string>
#include <time.h>





// the stream proxy source class
//     A stream proxy class is work as a proxy to relay the stream from the
// back-end source. 
// Thread safety: 
//     this class is not thread-safe for simpleness. 

class StreamProxySource
:public stream_switch::SourceListener, public stream_switch::SinkListener{
  
public:


	static StreamProxySource* Instance()
	{ 
		if ( NULL == s_instance )
		{
			s_instance = new StreamProxySource();
		}

		return s_instance;
	}

	static void Uninstance()
	{
		if ( NULL != s_instance )
		{
			delete s_instance;
			s_instance = NULL;
		}
	}




    virtual ~StreamProxySource();
    
    
    /**
     * @brief Init this proxy source
     * 
     * @param stsw_url  The stsw url of the back-end source
     * @param stream_name  The sting name of the stream output by this source
     * @param source_tcp_port The tcp port of this stream, 0 means no tcp port
     * @param sub_queue_size  The max size of the queue for subscriber to the back-end source
     * @param pub_queue_size  The max size of the queue for the publisher of this source
     * @param debug_flags  StreamSwitch source debug flags
     * @return 0 if sucessful, or other code of ErrorCode if failed 
     */
    virtual int Init(std::string stsw_url,
                     std::string stream_name,                       
                     int source_tcp_port,             
                     int sub_queue_size, 
                     int pub_queue_size,
                     int debug_flags);    

    virtual void Uninit();

    virtual bool IsInit();
    virtual bool IsStarted();
    virtual bool IsMetaReady();
    
    /**
     * @brief Get the metadata from the backend source 
     * @param timeout   in milli-second
     * @param err_info  output the error info if failed
     * @return 0 if sucessful, or other code of ErrorCode if failed 
     */
    virtual int UpdateStreamMetaData(int timeout, stream_switch::StreamMetadata * metadata);
    
    virtual int Start();
    
    virtual void Stop();
    
    virtual int Hearbeat();


    ///////////////////////////////////
    // SourceListener methods
    virtual void OnKeyFrame(void);
    virtual void OnMediaStatistic(stream_switch::MediaStatisticInfo *statistic);    


    ///////////////////////////////////
    // SinkListener methods
    virtual void OnLiveMediaFrame(const stream_switch::MediaFrameInfo &frame_info, 
                                  const char * frame_data, 
                                  size_t frame_size);
    virtual void OnMetadataMismatch(uint32_t mismatch_ssrc);  

 
       
private: 
    StreamProxySource();
    static StreamProxySource * s_instance;

    stream_switch::StreamSource * source_;
    stream_switch::StreamSink * sink_;
    
    pthread_mutex_t lock_;    
    bool need_key_frame_;
    bool need_update_metadata_;    
    time_t last_frame_recv_;
    
#define STREAM_PROXY_FLAG_INIT 1
#define STREAM_PROXY_FLAG_STARTED 2
#define STREAM_PROXY_FLAG_META_READY 4
    volatile uint32_t flags_;       



};
