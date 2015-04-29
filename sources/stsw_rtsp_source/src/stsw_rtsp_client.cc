/**
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
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
/**
 * stsw_rtsp_client.cc
 *      LiveRtspClient class implementation file, defines all the method of 
 * LiveRtspClient
 * 
 * author: jamken
 * date: 2015-4-1
**/ 

#include "stsw_rtsp_client.h"

#include "BasicUsageEnvironment.hh"
#undef _WIN32
#undef __WIN32__
#include "GroupsockHelper.hh"

#include "stsw_output_sink.h"
#include "stsw_pts_normalizer.h"

#if 0

#if defined(__WIN32__) || defined(_WIN32)
#define snprintf _snprintf
#else
#include <signal.h>
#define USE_SIGNALS 1
#endif


// Forward function definitions:
void continueAfterOPTIONS(RTSPClient* client, int resultCode, char* resultString);
void continueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* client, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* client, int resultCode, char* resultString);
void continueAfterTEARDOWN(RTSPClient* client, int resultCode, char* resultString);
void continueAfterKeepAlive(RTSPClient*, int resultCode, char* resultString);
 
void setupStreams();
void closeMediaSinks();
void closeLiveVideoFramer();
void closeAudioSource();


void subsessionByeHandler(void* clientData);

void sessionTimerHandler(void* clientData);

void sendRtspKeepAliveRequest(void* clientData);

void rtspClientConnectTimeout(void* clientData);

void signalHandlerShutdown(int sig);

void checkInterPacketGaps(void* clientData);
void beginQOSMeasurement();


void clientShutdown(int errorType);
void RtspRtpError(int errorType);
void RTSPConstructFail(int errorType);

Medium* createClient(UsageEnvironment& env, char const* url, int verbosityLevel, char const* applicationName);
void getOptions(RTSPClient::responseHandler* afterFunc);
extern void getSDPDescription(RTSPClient::responseHandler* afterFunc);
extern void setupSubsession(MediaSubsession* subsession, Boolean streamUsingTCP, RTSPClient::responseHandler* afterFunc);
extern void startPlayingSession(MediaSession* session, double start, double end, float scale, RTSPClient::responseHandler* afterFunc);
extern void tearDownSession(MediaSession* session, RTSPClient::responseHandler* afterFunc);





#endif


Boolean isRTSPConstructFail = True;
static Boolean forceMulticastOnUnspecified = False;
static double durationSlop = 1.0; // extra seconds to play at the end
static double initialSeekTime = 0.0f;
static char *initialAbsoluteSeekTime = NULL;
static float scale = 1.0f;
static unsigned interPacketGapMaxTime = 10; //check for each 10 second 
static unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
static int simpleRTPoffsetArg = -1;
static Boolean sendOptionsRequest = False;
static unsigned short desiredPortNum = 0;
extern unsigned qosMeasurementResetIntervalSec; // 0 means: Don't reset

static unsigned VideoSinkBufferSize = 1048576; /* 1M bytes */
static unsigned AudioSinkBufferSize = 131072; /* 128K bytes */
static unsigned socketVideoInputBufferSize = 2097152; /* 2M bytes */
static unsigned socketAudioInputBufferSize = 131072; /* 128K bytes */

#if 0
static H264VideoLiveSource *outputSource = NULL;
static MPEG4ESVideoLiveSource* mpeg4Source = NULL;
static SimpleRTPSource *outputAudioSource = NULL; 
#endif



RTSPClient* ourRTSPClient = NULL;
static char const* clientProtocolName = "RTSP";


static unsigned const rtpClientReorderWindowthresh = 20000; // 20 ms
Boolean areAlreadyShuttingDown = True;

//static RtspClientConstructCallback userConstructCallback = NULL;

//static RtspClientErrorCallback userErrorCallback = NULL;


#define RTSP_KEEPALIVE_INTERVAL 20000000

#define METADATA_SUBSESSION_RESERVE_NUM 10

const char* userAgent = "StreamSwitch";

///////////////////////////////////////////////////////////
//Public interfaces

LiveRtspClient::LiveRtspClient(UsageEnvironment& env, char const* rtspURL, 
			       Boolean streamUsingTCP, Boolean enableRtspKeepAlive, 
                   char const* singleMedium, 
                   char const* userName, char const* passwd,  
                   LiveRtspClientListener * listener, 
                   int verbosityLevel)
:RTSPClient(env, rtspURL, verbosityLevel, "stsw_rtsp_client", 0, -1), 
listener_(listener), 
are_already_shutting_down_(True), stream_using_tcp_(streamUsingTCP), 
enable_rtsp_keep_alive_(enableRtspKeepAlive), single_medium_(NULL), 
rtsp_url_(rtspURL), our_authenticator(NULL), 
session_(NULL), 
ssrc_(0), is_metadata_ok_(False), 
session_timer_task_(NULL), 
inter_packet_gap_check_timer_task_(NULL), qos_measurement_timer_task_(NULL),
tear_down_task_(NULL), rtsp_keep_alive_task_(NULL), rtsp_timeout_task_(NULL),
has_receive_keep_alive_response(False), duration_(0.0), endTime_(0.0), 
pts_session_normalizer(new PtsSessionNormalizer(env)),
made_progress_(False), setup_iter_(NULL), cur_setup_subsession_(NULL)

{
    if(singleMedium != NULL){
        single_medium_ = strdup(singleMedium);
    }
    client_start_time_.tv_sec = 0;
    client_start_time_.tv_usec = 0;
    
    if(userName != NULL) {
        our_authenticator = new Authenticator(userName,passwd);
    }  
    metadata_.source_proto = "RTSP"; 
    metadata_.play_type = stream_switch::STREAM_PLAY_TYPE_LIVE;
    metadata_.sub_streams.reserve(METADATA_SUBSESSION_RESERVE_NUM);
}


LiveRtspClient::~LiveRtspClient()
{
    if(single_medium_ != NULL){
        free(single_medium_);
        single_medium_ = NULL;
    }
    
    if(our_authenticator != NULL){
        delete our_authenticator;
        our_authenticator = NULL;
    }
    
    Medium::close(pts_session_normalizer);
}


int LiveRtspClient::Start()
{
    SetUserAgentString(userAgent);
    gettimeofday(&client_start_time_, NULL);
    
    srand(client_start_time_.tv_sec + client_start_time_.tv_usec);
    metadata_.ssrc = (uint32_t)(rand() % 0xffffffff);     
    
    //select a random ssrc
 
    if (sendOptionsRequest) {
        // Begin by sending an "OPTIONS" command:
        GetOptions(ContinueAfterOPTIONS);
    } else {
        ContinueAfterOPTIONS(this, 0, NULL);
    }  

    // start the timeout check 
  
    rtsp_timeout_task_ = envir().taskScheduler().scheduleDelayedTask(
        60000000 /* 1 minutes */, (TaskFunc*)RtspClientConnectTimeout, 
        (void*)this);

       
    are_already_shutting_down_ = False;
    
    return 0;
}


void LiveRtspClient::Shutdown()
{
    //if not start or already shutdown, just return
    if (are_already_shutting_down_) return; 
    are_already_shutting_down_ = True;
    
    //cancel the delay task
    
   // Teardown, then shutdown immediately
  if (session_ != NULL) {
    TearDownSession(session_, NULL);
  }

  ContinueAfterTEARDOWN(this, 0, NULL); 
    
}


void LiveRtspClient::CheckMetadata()
{
    using namespace stream_switch;
    if(is_metadata_ok_){
        //if OK already, just ignore
        return;
    }
    
    if(metadata_.ssrc == 0){
        //ssrc not ready
        return;
    }
    if(metadata_.source_proto.size() == 0){
        //protocol name not ready
        return;
    }
    
    //check subsesion number
    if(metadata_.sub_streams.size() == 0){
        //no subsessions
        return;
    }
    
    //check each subsession
    SubStreamMetadataVector::iterator it;
    for(it = metadata_.sub_streams.begin();
        it != metadata_.sub_streams.end();
        it++)
    {
        if(it->codec_name.size() == 0){
            //codec name not ready
            return; 
        }
        
        if(it->codec_name == "H264" || it->codec_name == "H265" ){
            if(it->extra_data.size() == 0){
                //extra_data must present
                return;
            }
        }
        
    }
    
    //check successful
    is_metadata_ok_ = True;
    if(listener_ != NULL){
        listener_->OnMetaReady(metadata_);
    }
}


void LiveRtspClient::AfterGettingFrame(int32_t sub_stream_index, 
                           stream_switch::MediaFrameType frame_type, 
                           struct timeval timestamp, 
                           unsigned frame_size, 
                           char * frame_buf)
{
    if(!IsMetaReady()){
        //metadata not ready, just drop the frame
        return;
    }
    if(listener_ != NULL){
        stream_switch::MediaFrameInfo frame_info;
        frame_info.frame_type = frame_type;
        frame_info.timestamp.tv_sec = timestamp.tv_sec;
        frame_info.timestamp.tv_usec = timestamp.tv_usec;
        frame_info.sub_stream_index = sub_stream_index;
        frame_info.ssrc = metadata_.ssrc;
        listener_->OnMediaFrame(frame_info, frame_buf, frame_size);
    }
    
}


////////////////////////////////////////////////////////////
//RTSP callback function

void LiveRtspClient::ContinueAfterOPTIONS(RTSPClient *client, 
    int resultCode, char* resultString)
{
    LiveRtspClient *my_client = dynamic_cast<LiveRtspClient *> (client);
    if(resultString != NULL){
        delete[] resultString;
        
    }

    if (resultCode != 0) {
        client->envir() << clientProtocolName 
                        << " \"OPTIONS\" request failed: " 
                        << resultString << "\n";
    } else {
        client->envir() << clientProtocolName 
                        << " \"OPTIONS\" request returned: " 
                        << resultString << "\n";
    }
    
    // Next, get a SDP description for the stream:
    my_client->GetSDPDescription(ContinueAfterDESCRIBE);
    
}

void LiveRtspClient::ContinueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString) 
{
    
    LiveRtspClient *my_client = dynamic_cast<LiveRtspClient *> (client);
    if (resultCode != 0) {
        my_client->envir() << "Failed to get a SDP description for the URL \"" 
            << my_client->rtsp_url_.c_str() << "\": " << resultString << "\n";
        delete[] resultString;
        my_client->HandleError(RTSP_CLIENT_ERR_DESCRIBE_ERR, 
            "Fail to get a SDP for DESCRIBE");  
        return;
    }

    char* sdpDescription = resultString;
        my_client->envir() << "Opened URL \"" << my_client->rtsp_url_.c_str() 
        << "\", returning a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    my_client->session_ = MediaSession::createNew(my_client->envir(), sdpDescription);
    delete[] sdpDescription;
    if (my_client->session_ == NULL) {
        my_client->envir() << "Failed to create a MediaSession object from the SDP description: " 
        << my_client->envir().getResultMsg() << "\n";
        my_client->HandleError(RTSP_CLIENT_ERR_MEDIASESSION_CREATE_FAIL, 
            "Failed to create a MediaSession from the SDP");  
        return;
    
    } else if (!my_client->session_->hasSubsessions()) {
        my_client->envir() << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
        my_client->HandleError(RTSP_CLIENT_ERR_NO_SUBSESSION, 
            "This session has no media subsessions"); 
        return;
    }

    // Then, setup the "RTPSource"s for the session:
    MediaSubsessionIterator iter(*(my_client->session_));
    MediaSubsession *subsession;
    Boolean madeProgress = False;
    char const* singleMediumToTest = my_client->single_medium_;
    while ((subsession = iter.next()) != NULL) {
        // If we've asked to receive only a single medium, then check this now:
        if (singleMediumToTest != NULL) {
            if (strcmp(subsession->mediumName(), singleMediumToTest) != 0) {
                my_client->envir() << "Ignoring \"" << subsession->mediumName()
                        << "/" << subsession->codecName()
                    << "\" subsession, because we've asked to receive a single " << singleMediumToTest
                    << " session only\n";
                continue;
            } else {
                // Receive this subsession only
                singleMediumToTest = "xxxxx";
                // this hack ensures that we get only 1 subsession of this type
            }
        }

        if (desiredPortNum != 0) {
            subsession->setClientPortNum(desiredPortNum);
            desiredPortNum += 2;
        }


        if (!subsession->initiate(simpleRTPoffsetArg)) {
            my_client->envir() << "Unable to create receiver for \"" << subsession->mediumName()
                << "/" << subsession->codecName()
                << "\" subsession: " << my_client->envir().getResultMsg() << "\n";
                
        } else {
            my_client->envir() << "Created receiver for \"" << subsession->mediumName()
                << "/" << subsession->codecName() << "\" subsession (";
            if (subsession->rtcpIsMuxed()) {
                my_client->envir() << "client port " << subsession->clientPortNum();
            } else {
                my_client->envir() << "client ports " << subsession->clientPortNum()
                    << "-" << subsession->clientPortNum()+1;
            }
            my_client->envir() << ")\n";
            madeProgress = True;
	
            if (subsession->rtpSource() != NULL) {
                // Because we're relaying the incoming data lively, rather than saving, 
                // should use an especially small time threshold, maybe 10ms is suitable?

                subsession->rtpSource()->setPacketReorderingThresholdTime(rtpClientReorderWindowthresh);
	  
        // Set the RTP source's OS socket buffer size as appropriate - either if we were explicitly asked (using -B),
        // or if the desired FileSink buffer size happens to be larger than the current OS socket buffer size.
        // (The latter case is a heuristic, on the assumption that if the user asked for a large FileSink buffer size,
        // then the input data rate may be large enough to justify increasing the OS socket buffer size also.)
                int socketNum = subsession->rtpSource()->RTPgs()->socketNum();
                unsigned curBufferSize = getReceiveBufferSize(my_client->envir(), socketNum);
                unsigned socket_input_buf = 0;
                unsigned sink_file_buf = 0;
                if(strcmp(subsession->mediumName(), "video") == 0){
                    socket_input_buf = socketVideoInputBufferSize;
                    sink_file_buf = VideoSinkBufferSize;
                }else{
                    socket_input_buf = socketAudioInputBufferSize;
                    sink_file_buf = AudioSinkBufferSize;                    
                }
                
                if (socket_input_buf > 0 || sink_file_buf > curBufferSize) {
                    unsigned newBufferSize = socket_input_buf > 0 ? socket_input_buf : sink_file_buf;
                    newBufferSize = setReceiveBufferTo(my_client->envir(), socketNum, newBufferSize);
                    if (socket_input_buf > 0) { // The user explicitly asked for the new socket buffer size; announce it:
                        my_client->envir() << "Changed socket receive buffer size for the \""
                            << subsession->mediumName()
                            << "/" << subsession->codecName()
                            << "\" subsession from "
                            << curBufferSize << " to "
                            << newBufferSize << " bytes\n";
                    }
                } //if (socket_input_buf > 0 || sink_file_buf > curBufferSize)
            }//if (subsession->rtpSource() != NULL)
        }//if (!subsession->initiate(simpleRTPoffsetArg))

    }//while ((subsession = iter.next()) != NULL)
    
    if (!madeProgress) {        
        my_client->HandleError(RTSP_CLIENT_ERR_SUBSESSION_INIT_ERR, 
            "Subsessions init error");          

    }

    // Perform additional 'setup' on each subsession, before playing them:
    my_client->made_progress_ = False;
    my_client->SetupStreams();

}


void LiveRtspClient::ContinueAfterSETUP(RTSPClient* client, int resultCode, char* resultString) {
    
    LiveRtspClient *my_client = dynamic_cast<LiveRtspClient *> (client);
    
    if (resultCode == 0) {
        my_client->envir() << "Setup \"" << my_client->cur_setup_subsession_->mediumName()
            << "/" << my_client->cur_setup_subsession_->codecName()
            << "\" subsession (";
        if (my_client->cur_setup_subsession_->rtcpIsMuxed()) {
            my_client->envir()  << "client port " << my_client->cur_setup_subsession_->clientPortNum();
        } else {
            my_client->envir()  << "client ports " << my_client->cur_setup_subsession_->clientPortNum()
                << "-" << my_client->cur_setup_subsession_->clientPortNum()+1;
        }
        my_client->envir()  << ")\n";
        my_client->made_progress_ = True;
    } else {
        my_client->envir()  << "Failed to setup \"" << my_client->cur_setup_subsession_->mediumName()
            << "/" << my_client->cur_setup_subsession_->codecName()
            << "\" subsession: " << resultString << "\n";
    }
    if(resultString != NULL) {
        delete[] resultString;
    }

    // Set up the next subsession, if any:
    my_client->SetupStreams();
}




int LiveRtspClient::SetupSinks()
{
    int ret = RTSP_CLIENT_ERR_NO_SUBSESSION;
    MediaSubsession *subsession;
    // Create and start "OutputSink"s for each subsession:
    MediaSubsessionIterator iter(*session_);
    int index = 0;
    while ((subsession = iter.next()) != NULL) {
        if (subsession->readSource() == NULL) continue; // was not initiated
 


        // Add to the front of all data sources a filter that will 'normalize' their frames' presentation times,
        // before the frames get re-transmitted by our server:
        char const* const codecName = subsession->codecName();
        char const* const mediaName = subsession->mediumName();
        FramedFilter* normalizerFilter = pts_session_normalizer->
            createNewPtsSubsessionNormalizer(subsession->readSource(), subsession->rtpSource(),
							mediaName, codecName);
        subsession->addFilter(normalizerFilter);

        // Some data sources require a 'framer' object to be added, before they can be fed into
        // a special "OutputSink".  Adjust for this now:
        if (strcmp(codecName, "H264") == 0) {
            subsession->addFilter(H264VideoStreamDiscreteFramer
					 ::createNew(envir(), subsession->readSource()));
        } else if (strcmp(codecName, "H265") == 0) {
            subsession->addFilter(H265VideoStreamDiscreteFramer
					 ::createNew(envir(), subsession->readSource()));
        } else if (strcmp(codecName, "MP4V-ES") == 0) {
            subsession->addFilter(MPEG4VideoStreamDiscreteFramer
					 ::createNew(envir(), subsession->readSource(),
						     True/* leave PTs unmodified*/));
        } else if (strcmp(codecName, "MPV") == 0) {
            subsession->addFilter(MPEG1or2VideoStreamDiscreteFramer
					 ::createNew(envir(), subsession->readSource(),
						     False, 5.0, True/* leave PTs unmodified*/));
        } else if (strcmp(codecName, "DV") == 0) {
            subsession->addFilter(DVVideoStreamFramer
					 ::createNew(envir(), subsession->readSource(),
						     False, True/* leave PTs unmodified*/));
        }
  


        MediaOutputSink* output_sink = NULL;

        if (strcmp(subsession->mediumName(), "video") == 0) {
            if (strcmp(subsession->codecName(), "H264") == 0) {
            // For H.264 video stream, we use a special sink that adds 'start codes',
            // and (at the start) the SPS and PPS NAL units:

            } else if (strcmp(subsession->codecName(), "H265") == 0) {
            // For H.265 video stream, we use a special sink that adds 'start codes',
            // and (at the start) the VPS, SPS, and PPS NAL units:

            } else if (strcmp(subsession->codecName(), "MP4V-ES") == 0) {
            // For H.265 video stream, we use a special sink :

                        
            
            }else{
                output_sink = MediaOutputSink::createNew(envir(), 
                                            this, 
                                            subsession, index, 
                                            VideoSinkBufferSize);                   
            }
        } else if (strcmp(subsession->mediumName(), "audio") == 0) {
            output_sink = MediaOutputSink::createNew(envir(), 
                                          this, 
                                          subsession, index, 
                                          AudioSinkBufferSize);            
        }
        if (output_sink == NULL) {
            // Normal case:
            output_sink = MediaOutputSink::createNew(envir(), 
                this, 
                subsession, index, 
                AudioSinkBufferSize);

        }

        subsession->miscPtr = this; // a hack to let subsession handler functions get the "RTSPClient" from the subsession        

        subsession->sink = output_sink;
        
        if (subsession->sink == NULL) {
            envir() << "Failed to create MediaOuputSink for the \"" << subsession->mediumName()
            << "/" << subsession->codecName()
            << "\" subsession: " << envir().getResultMsg() << "\n";
            continue;//next substream
        } else {

            envir() << "Created a output sink for the \"" << subsession->mediumName()
            << "/" << subsession->codecName()
            << "\" subsession \n";
        }        
        subsession->sink->startPlaying(*(subsession->readSource()),
                        SubsessionAfterPlaying,
                        subsession);
	
        // Also set a handler to be called if a RTCP "BYE" arrives
        // for this subsession:
        if (subsession->rtcpInstance() != NULL) {
            subsession->rtcpInstance()->setByeHandler(SubsessionByeHandler, subsession);
        }
        
        index++;
        ret = RTSP_CLIENT_ERR_SUCCESSFUL;
    }//  while ((subsession = iter.next()) != NULL) {
    
    return ret;
    
}



void LiveRtspClient::SetupStreams() {
    MediaSubsession *subsession;
    if (setup_iter_ == NULL) setup_iter_ = new MediaSubsessionIterator(*session_);
    while ((subsession = setup_iter_->next()) != NULL) {
        // We have another subsession left to set up:
        if (subsession->clientPortNum() == 0) continue; // port # was not set
        
        cur_setup_subsession_ = subsession;
        SetupSubsession(subsession, stream_using_tcp_, forceMulticastOnUnspecified, ContinueAfterSETUP);
        return;
    }

    // We're done setting up subsessions.
    delete setup_iter_;
    setup_iter_ = NULL;
    if (!made_progress_) {
        HandleError(RTSP_CLIENT_ERR_SETUP_ERR, 
        "All subsessions terminated");  
        return;
    }

    //Create sink
    int ret;
    ret = SetupSinks();
    if(ret){
        HandleError(RTSP_CLIENT_ERR_SUBSESSION_INIT_ERR, 
        "Subsessions Init error");  
        return;
    }


    //setup metadata from session, 
    //and call back user function if ready. 
    SetupMetaFromSession();


  // Finally, start playing each subsession, to start the data flow:
    if (duration_ == 0) {
        if (scale > 0) duration_ = session_->playEndTime() - initialSeekTime; // use SDP end time
        else if (scale < 0) duration_ = initialSeekTime;
    }
    if (duration_ < 0) duration_ = 0.0;

    endTime_ = initialSeekTime;
    if (scale > 0) {
        if (duration_ <= 0) endTime_ = -1.0f;
        else endTime_ = initialSeekTime + duration_;
    } else {
        endTime_ = initialSeekTime - duration_;
        if (endTime_ < 0) endTime_ = 0.0f;
    } 

    char const* absStartTime = initialAbsoluteSeekTime != NULL ? initialAbsoluteSeekTime : session_->absStartTime();
    if (absStartTime != NULL) {
        // Either we or the server have specified that seeking should be done by 'absolute' time:
        StartPlayingSession(session_, absStartTime, session_->absEndTime(), scale, ContinueAfterPLAY);
    } else {
        // Normal case: Seek by relative time (NPT):
        StartPlayingSession(session_, initialSeekTime, endTime_, scale, ContinueAfterPLAY);
    }
}







void LiveRtspClient::ContinueAfterPLAY(RTSPClient* client, int resultCode, char* resultString) 
{
    
    LiveRtspClient *my_client = dynamic_cast<LiveRtspClient *> (client);
    if(resultString != NULL) {
        delete[] resultString;
    }    
    if (resultCode != 0) {
        my_client->envir() << "Failed to start playing session: " << resultString << "\n";
        my_client->HandleError(RTSP_CLIENT_ERR_PLAY_ERR, 
            "RTSP Play error");  
        return;
    } else {
        my_client->envir() << "Started playing session\n";
    }


    // Figure out how long to delay (if at all) before shutting down, or
    // repeating the playing
    Boolean timerIsBeingUsed = False;
    double secondsToDelay = my_client->duration_;
    if (my_client->duration_ > 0) {
        // First, adjust "duration" based on any change to the play range 
        // (that was specified in the "PLAY" response):
        double rangeAdjustment = 
            (my_client->session_->playEndTime() - my_client->session_->playStartTime()) - 
            (my_client->endTime_ - initialSeekTime);
        if (my_client->duration_ + rangeAdjustment > 0.0) {
            my_client->duration_ += rangeAdjustment;
        }

        timerIsBeingUsed = True;
        double absScale = scale > 0 ? scale : -scale; // ASSERT: scale != 0
        secondsToDelay = my_client->duration_/absScale + durationSlop;

        int64_t uSecsToDelay = (int64_t)(secondsToDelay*1000000.0);
        my_client->session_timer_task_ = 
            my_client->envir().taskScheduler().scheduleDelayedTask(
            uSecsToDelay, (TaskFunc*)SessionTimerHandler, (void*)NULL);
    }

    char const* actionString =  "Receiving streamed data";
    if (timerIsBeingUsed) {
        my_client->envir() << actionString
            << " (for up to " << secondsToDelay
            << " seconds)...\n";
    } else {
        my_client->envir() << actionString << "...\n";

    }

  // Watch for incoming packets (if desired):
  //checkForPacketArrival(NULL);
  //checkInterPacketGaps(NULL);
  //checkSessionTimeoutBrokenServer(NULL);
  
    if(my_client->rtsp_timeout_task_ != NULL) {
        my_client->envir().taskScheduler().unscheduleDelayedTask(
            my_client->rtsp_timeout_task_);
        my_client->rtsp_timeout_task_ = NULL;
    }  
    
    if(my_client->enable_rtsp_keep_alive_ == True){
        my_client->KeepAliveSession(my_client->session_, ContinueAfterKeepAlive);    
    }
  
  
    //callback user function
    if(my_client->listener_ != NULL){
        my_client->listener_->OnRtspOK();
    }
}


void LiveRtspClient::ContinueAfterKeepAlive(RTSPClient* client, int resultCode, char* resultString) 
{
    LiveRtspClient *my_client = dynamic_cast<LiveRtspClient *> (client);
    if(resultString != NULL) {
        delete[] resultString;
    }
    
    if(my_client->session_ != NULL && my_client->rtsp_keep_alive_task_ == NULL){
        unsigned sessionTimeoutParameter = my_client->sessionTimeoutParameter();
        unsigned sessionTimeout = sessionTimeoutParameter == 0 ? 60/*default*/ : sessionTimeoutParameter;
        unsigned secondsUntilNextKeepAlive = sessionTimeout <= 5 ? 1 : sessionTimeout - 5;
        // Reduce the interval a little, to be on the safe side

        my_client->rtsp_keep_alive_task_ 
            = my_client->envir().taskScheduler().scheduleDelayedTask(
            secondsUntilNextKeepAlive*1000000, (TaskFunc*)RtspKeepAliveHandler, my_client);
        
    }

}

void LiveRtspClient::ContinueAfterTEARDOWN(RTSPClient* client, 
    int /*resultCode*/, char* resultString) 
{
    LiveRtspClient *my_client = dynamic_cast<LiveRtspClient *> (client);
    if(resultString != NULL) {
        delete[] resultString;
    }

    // Now that we've stopped any more incoming data from arriving, close our output files:
    my_client->closeMediaSinks();
    Medium::close(my_client->session_);
    my_client->session_ = NULL;

}


void LiveRtspClient::SubsessionAfterPlaying(void* clientData) {
    // Begin by closing this media subsession's stream:
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    LiveRtspClient *my_client = (LiveRtspClient *)subsession->miscPtr;
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL) return; // this subsession is still active
    }

  // All subsessions' streams have now been closed
  //error handle
    my_client->HandleError(RTSP_CLIENT_ERR_SUBSESSION_BYE, 
        "All subsessions have been closed");  
}


////////////////////////////////////////////////////////////
//RTSP command sending functions

void LiveRtspClient::GetOptions(RTSPClient::responseHandler* afterFunc) 
{ 
    sendOptionsCommand(afterFunc, our_authenticator);
}

void LiveRtspClient::GetSDPDescription(RTSPClient::responseHandler* afterFunc) 
{
    sendDescribeCommand(afterFunc, our_authenticator);
}

void LiveRtspClient::SetupSubsession(MediaSubsession* subsession, 
    Boolean streamUsingTCP, Boolean forceMulticastOnUnspecified, 
    RTSPClient::responseHandler* afterFunc) 
{
  
    sendSetupCommand(*subsession, afterFunc, False, streamUsingTCP, 
        forceMulticastOnUnspecified, our_authenticator);
}

void LiveRtspClient::StartPlayingSession(MediaSession* session, 
    double start, double end, float scale, 
    RTSPClient::responseHandler* afterFunc) 
{
    
    sendPlayCommand(*session, afterFunc, start, end, scale, our_authenticator);
}

void LiveRtspClient::StartPlayingSession(MediaSession* session, 
    char const* absStartTime, char const* absEndTime, 
    float scale, RTSPClient::responseHandler* afterFunc) 
{
    sendPlayCommand(*session, afterFunc, absStartTime, 
        absEndTime, scale, our_authenticator);
}

void LiveRtspClient::TearDownSession(MediaSession* session,
    RTSPClient::responseHandler* afterFunc) 
{
    sendTeardownCommand(*session, afterFunc, our_authenticator);
}

void LiveRtspClient::KeepAliveSession(MediaSession* session, 
        RTSPClient::responseHandler* afterFunc)
{
    sendSetParameterCommand(*session, afterFunc, "Ping", "Pong", our_authenticator);
}



////////////////////////////////////////////////////////////
//Timer task

void LiveRtspClient::RtspKeepAliveHandler(void* clientData)
{
    LiveRtspClient *my_client = (LiveRtspClient *)clientData;
    my_client->rtsp_keep_alive_task_ = NULL;
        
    if(my_client->session_ != NULL) {
        my_client->KeepAliveSession(my_client->session_, ContinueAfterKeepAlive);
    }

}


void LiveRtspClient::SessionTimerHandler(void* clientData)
{
    LiveRtspClient *my_client = (LiveRtspClient *)clientData;
    my_client->session_timer_task_ = NULL;
    
    //TODO: error handle 
    my_client->HandleError(RTSP_CLIENT_ERR_SESSION_TIMER, 
        "Rtsp session duration due");    
}

void LiveRtspClient::SubsessionByeHandler(void* clientData)
{
    MediaSubsession* subsession = (MediaSubsession*)clientData;   
    LiveRtspClient *my_client = (LiveRtspClient *)subsession->miscPtr;

    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    unsigned secsDiff = 
        timeNow.tv_sec - my_client->client_start_time_.tv_sec;

    my_client->envir() << "Received RTCP \"BYE\" on \"" << subsession->mediumName()
        << "/" << subsession->codecName()
        << "\" subsession (after " << secsDiff
        << " seconds)\n";

    // Act now as if the subsession had closed:
    SubsessionAfterPlaying(subsession);    
}


void LiveRtspClient::RtspClientConnectTimeout(void* clientData)
{
    LiveRtspClient *my_client = (LiveRtspClient *)clientData;
    my_client->rtsp_timeout_task_ = NULL;
    
    my_client->envir() << "Rtsp negotiation time out\n";
    
    my_client->HandleError(RTSP_CLIENT_ERR_CONNECT_FAIL, 
        "Rtsp negotiation time out");
    return;        
}


////////////////////////////////////////////////////////////
//Utils


void LiveRtspClient::SetUserAgentString(char const* userAgentString) 
{
    SetUserAgentString(userAgentString);
}

void LiveRtspClient::closeMediaSinks()
{
  if (session_ == NULL) return;
    MediaSubsessionIterator iter(*session_);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
        Medium::close(subsession->sink);
        subsession->sink = NULL;
    }    
}


void LiveRtspClient::SetupMetaFromSession()
{
    using namespace stream_switch;
    if(session_ == NULL){
        return; //no session yet
    }
    uint32_t bps = 0; 
    
    MediaSubsession *subsession;
    MediaSubsessionIterator iter(*session_);
    int index = 0;
    while ((subsession = iter.next()) != NULL) {
        if (subsession->readSource() == NULL) continue; // was not initiated

        char const* const codecName = subsession->codecName();
        char const* const mediaName = subsession->mediumName();    
        
        if(strcmp(codecName, "MP2P") == 0 ||
           strcmp(codecName, "MP2T") == 0 ){
            //this is a mux stream, cannot setup metadata from sdp
            metadata_.sub_streams.clear(); // no sub stream available
            return;
        }
        metadata_.sub_streams[index].sub_stream_index = index;
        metadata_.sub_streams[index].codec_name = codecName;
        metadata_.sub_streams[index].direction = SUB_STREAM_DIRECTION_OUTBOUND;
        
        if(strcmp(mediaName, "video") == 0){
            metadata_.sub_streams[index].media_type = 
                SUB_STREAM_MEIDA_TYPE_VIDEO;
            metadata_.sub_streams[index].media_param.video.height = 
                subsession->videoHeight();
            metadata_.sub_streams[index].media_param.video.width = 
                subsession->videoWidth(); 
            metadata_.sub_streams[index].media_param.video.fps = 
                subsession->videoFPS(); 
            
            if(strcmp(codecName, "H264") == 0){
                char const* sPropParameterSetsStr = 
                    subsession->fmtp_spropparametersets();
                char const start_code[4] = {0x00, 0x00, 0x00, 0x01};
                unsigned numSPropRecords;
                SPropRecord* sPropRecords
                    = parseSPropParameterSets(sPropParameterSetsStr, 
                            numSPropRecords);
                for (unsigned i = 0; i < numSPropRecords; ++i) {
                    metadata_.sub_streams[index].extra_data.append(start_code, 4);
                    metadata_.sub_streams[index].extra_data.append(
                        (const char *)sPropRecords[i].sPropBytes, (size_t)sPropRecords[i].sPropLength);

                }
                delete[] sPropRecords;
                
            }else if(strcmp(codecName, "H265") == 0){
                char const start_code[4] = {0x00, 0x00, 0x00, 0x01};
		        char const* fSPropParameterSetsStr[3];
                fSPropParameterSetsStr[0] = subsession->fmtp_spropvps();
                fSPropParameterSetsStr[1] = subsession->fmtp_spropsps();
                fSPropParameterSetsStr[2] = subsession->fmtp_sproppps();    
                
                for (unsigned j = 0; j < 3; ++j) {
                    unsigned numSPropRecords;
                    SPropRecord* sPropRecords
                        = parseSPropParameterSets(fSPropParameterSetsStr[j], 
                            numSPropRecords);
                    for (unsigned i = 0; i < numSPropRecords; ++i) {
                        metadata_.sub_streams[index].extra_data.append(start_code, 4);
                        metadata_.sub_streams[index].extra_data.append(
                            (const char *)sPropRecords[i].sPropBytes, 
                            (size_t)sPropRecords[i].sPropLength);
                    }
                    delete[] sPropRecords;
                }               
                
                
            }else if(strcmp(codecName, "MP4V-ES") == 0){
                unsigned configSize = 0;
                unsigned char* config = 
                    parseGeneralConfigStr(subsession->fmtp_config(), configSize);
                if(configSize != 0 && config != NULL){
                    metadata_.sub_streams[index].extra_data.assign(
                        (const char *)config, (size_t)configSize);
                }   
            }
               
        }else if(strcmp(mediaName, "audio") == 0){
            metadata_.sub_streams[index].media_type = 
                SUB_STREAM_MEIDA_TYPE_AUDIO;           
            metadata_.sub_streams[index].media_param.audio.channels = 1;
            if(subsession->numChannels() != 0 ){
                metadata_.sub_streams[index].media_param.audio.channels = 
                    subsession->numChannels();
            }
        }else if(strcmp(mediaName, "text") == 0){
            metadata_.sub_streams[index].media_type = 
                SUB_STREAM_MEIDA_TYPE_TEXT;            
        }else{
            metadata_.sub_streams[index].media_type = 
                SUB_STREAM_MEIDA_TYPE_PRIVATE;            
        }

        
        
        bps + subsession->bandwidth() * 1000;
        
        index++;
    } 
    metadata_.bps = bps;
    
    CheckMetadata();
        
}



/////////////////////////////////////////////////////////
//error handler        
void LiveRtspClient::HandleError(RtspClientErrCode err_code, 
        const char * err_info)
{
    //When encounter an error, shutdown this client
    Shutdown();
    
    if(listener_ != NULL){
        listener_->OnError(err_code, err_info);
    }
    
    
}



