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


extern UsageEnvironment* env;
extern char *  g_pszCodecName;
static Medium* ourClient = NULL;
static Authenticator* ourAuthenticator = NULL;
static char const* streamURL = NULL;
static MediaSession* session = NULL;
static TaskToken sessionTimerTask = NULL;
static TaskToken interPacketGapCheckTimerTask = NULL;
static TaskToken qosMeasurementTimerTask = NULL;
static TaskToken tearDownTask = NULL;
static TaskToken rtspKeepAliveTask = NULL;
static TaskToken rtspTimeoutTask = NULL;
Boolean enableRtspKeepAlive = False;
static Boolean hasReceiveKeepAliveResponse = False;
Boolean isRTSPConstructFail = True;
extern char const* singleMedium ;
extern int verbosityLevel; // by default, print verbose output
static double duration = 0;
static double durationSlop = -1.0; // extra seconds to play at the end
static double initialSeekTime = 0.0f;
static float scale = 1.0f;
static double endTime;
static unsigned interPacketGapMaxTime = 10; //check for each 10 second 
static unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
static int simpleRTPoffsetArg = -1;
static Boolean sendOptionsRequest = False;
extern Boolean streamUsingTCP;
static unsigned short desiredPortNum = 0;
static portNumBits tunnelOverHTTPPortNum = 0;
static unsigned qosMeasurementIntervalMS = 1000; // 0 means: Don't output QOS data
extern unsigned qosMeasurementResetIntervalSec; // 0 means: Don't reset

extern unsigned sinkBufferSize;
static unsigned socketVideoInputBufferSize = 2097152; /* 2M bytes */
static unsigned socketAudioInputBufferSize = 131072; /* 128K bytes */

static H264VideoLiveSource *outputSource = NULL;
static MPEG4ESVideoLiveSource* mpeg4Source = NULL;
static SimpleRTPSource *outputAudioSource = NULL; 


static struct timeval startTime;

RTSPClient* ourRTSPClient = NULL;
static char const* clientProtocolName = "RTSP";


static unsigned const rtpClientReorderWindowthresh = 20000; // 20 ms
Boolean areAlreadyShuttingDown = True;

static RtspClientConstructCallback userConstructCallback = NULL;

static RtspClientErrorCallback userErrorCallback = NULL;


#define RTSP_KEEPALIVE_INTERVAL 20000000


extern unsigned g_nTotalFrameNum ;

extern unsigned g_nIframeNum ;

extern unsigned g_nIntervalNumOfIframe;


#endif

LiveRtspClient::LiveRtspClient(UsageEnvironment& env, char const* rtspURL, 
			       Boolean streamUsingTCP, Boolean enableRtspKeepAlive, 
                   char const* singleMedium, int verbosityLevel)
:RTSPClient(env, rtspURL, verbosityLevel, "stsw_rtsp_client", 0, -1), 
are_already_shutting_down_(True), stream_using_tcp_(streamUsingTCP), 
enable_rtsp_keep_alive_(enableRtspKeepAlive), single_medium_(NULL)
{
    if(singleMedium != NULL){
        single_medium_ = strdup(singleMedium);
    }
}


LiveRtspClient::~LiveRtspClient()
{
    if(single_medium_ != NULL){
        free(single_medium_);
        single_medium_ = NULL;
    }
}

#if 0
int startRtspClient(char const* url, char const* progName, 
                    char const* userName, char const* passwd,
                    RtspClientConstructCallback constructCallback, 
                    RtspClientErrorCallback errorCallback)
{
  if(env == NULL) {
    return -1;
  }
  
  /* clean the following variable */
  areAlreadyShuttingDown = False;
  userConstructCallback = constructCallback;
  userErrorCallback = errorCallback;
  outputSource = NULL; 
  mpeg4Source = NULL;
  outputAudioSource = NULL; 

  
  
  if(userName != NULL) {
    ourAuthenticator = new Authenticator(userName,passwd);
  }
  
  gettimeofday(&startTime, NULL);
	
  streamURL = url;  
  // Create our client object:
  ourClient = createClient(*env, url, verbosityLevel, progName);
  if (ourClient == NULL) {
		
    *env << "Failed to create " << "RTSP"
		<< " client: " << env->getResultMsg() << "\n";
    if(ourAuthenticator != NULL) {
            delete ourAuthenticator;
            ourAuthenticator = NULL;
    }
		
    areAlreadyShuttingDown = True;
    userConstructCallback = NULL;
    userErrorCallback = NULL;		
		
    return -1;
  }

  if (sendOptionsRequest) {
    // Begin by sending an "OPTIONS" command:
    getOptions(continueAfterOPTIONS);
  } else {
    continueAfterOPTIONS(NULL, 0, NULL);
  }  

  // start the timeout check 
  rtspTimeoutTask = env->taskScheduler().scheduleDelayedTask(60000000 /* 1 minutes */, (TaskFunc*)rtspClientConnectTimeout, (void*)NULL);

  
  return 0;  
  
}
void rtspClientConnectTimeout(void* /*clientData*/)
{
    rtspTimeoutTask = NULL;
    *env << "Rtsp negotiation time out\n";
    RTSPConstructFail(RELAY_CLIENT_RESULT_CONNECT_FAIL);
    return;
}

Medium* createClient(UsageEnvironment& env, char const* url, int verbosityLevel, char const* applicationName) {
  return ourRTSPClient = RTSPClient::createNew(env, url, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

void getOptions(RTSPClient::responseHandler* afterFunc) { 
  ourRTSPClient->sendOptionsCommand(afterFunc, ourAuthenticator);
}

void getSDPDescription(RTSPClient::responseHandler* afterFunc) {
  ourRTSPClient->sendDescribeCommand(afterFunc, ourAuthenticator);
}

void setupSubsession(MediaSubsession* subsession, Boolean streamUsingTCP, RTSPClient::responseHandler* afterFunc) {
  Boolean forceMulticastOnUnspecified = False;
  ourRTSPClient->sendSetupCommand(*subsession, afterFunc, False, streamUsingTCP, forceMulticastOnUnspecified, ourAuthenticator);
}

void startPlayingSession(MediaSession* session, double start, double end, float scale, RTSPClient::responseHandler* afterFunc) {
  ourRTSPClient->sendPlayCommand(*session, afterFunc, start, end, scale, ourAuthenticator);
}

void tearDownSession(MediaSession* session, RTSPClient::responseHandler* afterFunc) {
  ourRTSPClient->sendTeardownCommand(*session, afterFunc, ourAuthenticator);
}

void keepAliveSession(MediaSession* session, RTSPClient::responseHandler* afterFunc) {
  ourRTSPClient->sendSetParameterCommand(*session, afterFunc, "Ping", "Pong", ourAuthenticator);
}


void continueAfterOPTIONS(RTSPClient*, int resultCode, char* resultString) {

	if (resultCode != 0) {
		*env << clientProtocolName << " \"OPTIONS\" request failed: " << resultString << "\n";
	} else {
		*env << clientProtocolName << " \"OPTIONS\" request returned: " << resultString << "\n";
	}

	if(resultString != NULL) {
		delete[] resultString;
	}

  // Next, get a SDP description for the stream:
  getSDPDescription(continueAfterDESCRIBE);
}

void continueAfterDESCRIBE(RTSPClient*, int resultCode, char* resultString) {
  if (resultCode != 0) {
    *env << "Failed to get a SDP description from URL \"" << streamURL << "\": " << resultString << "\n";
    RTSPConstructFail(RELAY_CLIENT_RESULT_DESCRIBE_ERR);
    return;
  }

  char* sdpDescription = resultString;
  *env << "Opened URL \"" << streamURL << "\", returning a SDP description:\n" << sdpDescription << "\n";

  // Create a media session object from this SDP description:
  session = MediaSession::createNew(*env, sdpDescription);
  delete[] sdpDescription;
  if (session == NULL) {
    *env << "Failed to create a MediaSession object from the SDP description: " << env->getResultMsg() << "\n";
    RTSPConstructFail(RELAY_CLIENT_RESULT_MEDIASESSION_CREATE_FAIL);
    return;
  } else if (!session->hasSubsessions()) {
    *env << "This session has no media subsessions (i.e., \"m=\" lines)\n";
    RTSPConstructFail(RELAY_CLIENT_RESULT_NO_SUBSESSION);
    return;
  }

  // Then, setup the "RTPSource"s for the session:
  MediaSubsessionIterator iter(*session);
  MediaSubsession *subsession;
  Boolean madeProgress = False;
  char const* singleMediumToTest = singleMedium;
  while ((subsession = iter.next()) != NULL) {
  	 static int nCount = 0;
  	 if((strcmp(subsession->mediumName(), "video") == 0) && (0== nCount))
	 {
	 	nCount++;
  		g_pszCodecName = new char[strlen(subsession->codecName())+1];
		snprintf(g_pszCodecName,sizeof(g_pszCodecName), "%s",subsession->codecName());
  	 }
    // If we've asked to receive only a single medium, then check this now:
    if (singleMediumToTest != NULL) {
      if (strcmp(subsession->mediumName(), singleMediumToTest) != 0) {
				*env << "Ignoring \"" << subsession->mediumName()
				<< "/" << subsession->codecName()
				<< "\" subsession, because we've asked to receive a single " << singleMedium
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
        *env << "Unable to create receiver for \"" << subsession->mediumName()
        << "/" << subsession->codecName()
        << "\" subsession: " << env->getResultMsg() << "\n";
    } else {
        *env << "Created receiver for \"" << subsession->mediumName()
        << "/" << subsession->codecName()
        << "\" subsession (client ports " << subsession->clientPortNum()
        << "-" << subsession->clientPortNum()+1 << ")\n";
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
            unsigned curBufferSize = getReceiveBufferSize(*env, socketNum);

            if(strcmp(subsession->mediumName(), "video") == 0){
				if (socketVideoInputBufferSize > 0 || sinkBufferSize > curBufferSize) {
					unsigned newBufferSize = socketVideoInputBufferSize > 0 ? socketVideoInputBufferSize : sinkBufferSize;
					newBufferSize = setReceiveBufferTo(*env, socketNum, newBufferSize);
					if (socketVideoInputBufferSize > 0) { // The user explicitly asked for the new socket buffer size; announce it:
						*env << "Changed socket receive buffer size for the \""
							<< subsession->mediumName()
							<< "/" << subsession->codecName()
							<< "\" subsession from "
							<< curBufferSize << " to "
							<< newBufferSize << " bytes\n";
					}
				}
            }else if(strcmp(subsession->mediumName(), "audio") == 0){
				if (socketAudioInputBufferSize > 0 || sinkBufferSize > curBufferSize) {
					unsigned newBufferSize = socketAudioInputBufferSize > 0 ? socketAudioInputBufferSize : sinkBufferSize;
					newBufferSize = setReceiveBufferTo(*env, socketNum, newBufferSize);
					if (socketAudioInputBufferSize > 0) { // The user explicitly asked for the new socket buffer size; announce it:
						*env << "Changed socket receive buffer size for the \""
							<< subsession->mediumName()
							<< "/" << subsession->codecName()
							<< "\" subsession from "
							<< curBufferSize << " to "
							<< newBufferSize << " bytes\n";
					}
				}


            }else{
                *env << "We cannot change socket receive buffer size for the \""
                    << subsession->mediumName()
                << "/" << subsession->codecName()
                << "\" subsession, because it's not a video/audio stream\n ";

            }
        }
    }
  }
  if (!madeProgress) {
    RTSPConstructFail(RELAY_CLIENT_RESULT_SUBSESSION_INIT_ERR);
    return;
  }

  // Perform additional 'setup' on each subsession, before playing them:
  setupStreams();
}

MediaSubsession *subsession;
Boolean madeProgress = False;
void continueAfterSETUP(RTSPClient*, int resultCode, char* resultString) {
  if (resultCode == 0) {
      *env << "Setup \"" << subsession->mediumName()
	   << "/" << subsession->codecName()
	   << "\" subsession (client ports " << subsession->clientPortNum()
	   << "-" << subsession->clientPortNum()+1 << ")\n";
      madeProgress = True;
  } else {
    *env << "Failed to setup \"" << subsession->mediumName()
	 << "/" << subsession->codecName()
	 << "\" subsession: " << env->getResultMsg() << "\n";
  }

  // Set up the next subsession, if any:
  setupStreams();
}

void setupStreams() {
  static MediaSubsessionIterator* setupIter = NULL;
  if (setupIter == NULL) setupIter = new MediaSubsessionIterator(*session);
  while ((subsession = setupIter->next()) != NULL) {
    // We have another subsession left to set up:
    if (subsession->clientPortNum() == 0) continue; // port # was not set

    setupSubsession(subsession, streamUsingTCP, continueAfterSETUP);
    return;
  }

  // We're done setting up subsessions.
  delete setupIter;
  setupIter = NULL;
	
  if (!madeProgress) {
    RTSPConstructFail(RELAY_CLIENT_RESULT_SETUP_ERR);
    return;
  }

  /* create the h264 live Frame Filter attached with the RTPsouce in the subsession */
  madeProgress = False;
  MediaSubsessionIterator iter(*session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->readSource() == NULL) continue; // was not initiated

    if (strcmp(subsession->mediumName(), "video") == 0 &&
        (strcmp(subsession->codecName(), "H264") == 0)) {
      // For H.264 video stream, we create the h264 live frame filter on it:
			/* for debug output */
			(*env) << "Create the H264VideoLiveSource instance with the following param: "
				     << "Width:" << subsession->videoWidth() <<", Height:"<<subsession->videoHeight()<<" "
				     << "FPS:"<<subsession->videoFPS()<<" "
				     << "spro_parametersets:"<<subsession->fmtp_spropparametersets()<<"\n";
			
      outputSource = H264VideoLiveSource::createNew((*env), subsession->readSource(),
                                                    subsession->videoWidth(), 
                                                    subsession->videoHeight(),
                                                    subsession->videoFPS(),
                                                    subsession->fmtp_spropparametersets());
      
      if(outputSource == NULL) {
        *env << "Failed to creat the H264LiveVideoSource for the video subsession\n ";
        RTSPConstructFail(RELAY_CLIENT_RESULT_RESOUCE_ERR);
        return;
      }

      // Also set a handler to be called if a RTCP "BYE" arrives
      // for this subsession:
      if (subsession->rtcpInstance() != NULL) {
          subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, subsession);
      } 

      madeProgress = True;   
      
    }
     if (strcmp(subsession->mediumName(), "video") == 0 &&
        (strcmp(subsession->codecName(), "MP4V-ES") == 0)) {
      // For MP4V-ES video stream, we create the MP4V-ES live frame filter on it:
			/* for debug output */
			(*env) << "Create the MP4VESVideoLiveSource instance with the following param: "
				     << "Width:" << subsession->videoWidth() <<", Height:"<<subsession->videoHeight()<<" "
				     << "FPS:"<<subsession->videoFPS()<<" "
				     << "spro_parametersets:"<<subsession->fmtp_spropparametersets()<<"\n";
     
      mpeg4Source = MPEG4ESVideoLiveSource::createNew((*env), subsession->readSource(),
                                                    subsession->videoWidth(), 
                                                    subsession->videoHeight(),
                                                    subsession->videoFPS(),
                                                    subsession->fmtp_profile_level_id(), 
                                                    subsession->fmtp_config());
      
      if(mpeg4Source == NULL) {
        *env << "Failed to creat the MP4VESLiveVideoSource for the video subsession\n ";
        RTSPConstructFail(RELAY_CLIENT_RESULT_RESOUCE_ERR);
        return;
      }

      // Also set a handler to be called if a RTCP "BYE" arrives
      // for this subsession:
      if (subsession->rtcpInstance() != NULL) {
          subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, subsession);
      } 

      madeProgress = True;   
      
    }

    if (strcmp(subsession->mediumName(), "audio") == 0 &&
        ((strcmp(subsession->codecName(), "PCMU") == 0) ||
        (strcmp(subsession->codecName(), "PCMA") == 0)) ) {
      // For G.711 audio stream, we get the simple rtp source from the subsssion:
		
      outputAudioSource = (SimpleRTPSource *)subsession->rtpSource();
      
      if(outputAudioSource == NULL) {
        *env << "Failed to get simple RTP source for the audio subsession\n ";
        RTSPConstructFail(RELAY_CLIENT_RESULT_RESOUCE_ERR);
        return;
      }

      // Also set a handler to be called if a RTCP "BYE" arrives
      // for this subsession:
      if (subsession->rtcpInstance() != NULL) {
          subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, subsession);
      } 

      //madeProgress = True;   
      
    }
  }
  
  if (!madeProgress) {
    RTSPConstructFail(RELAY_CLIENT_RESULT_NO_SUBSESSION);
    return;
  }  



  // Finally, start playing each subsession, to start the data flow:
  if (duration == 0) {
    if (scale > 0) duration = session->playEndTime() - initialSeekTime; // use SDP end time
    else if (scale < 0) duration = initialSeekTime;
  }
  if (duration < 0) duration = 0.0;

  endTime = initialSeekTime;
  if (scale > 0) {
    if (duration <= 0) endTime = -1.0f;
    else endTime = initialSeekTime + duration;
  } else {
    endTime = initialSeekTime - duration;
    if (endTime < 0) endTime = 0.0f;
  }

  startPlayingSession(session, initialSeekTime, endTime, scale, continueAfterPLAY);
}

void continueAfterPLAY(RTSPClient*, int resultCode, char* resultString) {
  if (resultCode != 0) {
    *env << "Failed to start playing session: " << resultString << "\n";
    RTSPConstructFail(RELAY_CLIENT_RESULT_PLAY_ERR);
    return;
  } else {
    *env << "Started playing session\n";
  }


  if (qosMeasurementIntervalMS > 0) {
    // Begin periodic QOS measurements:
    beginQOSMeasurement();
  }


  // Figure out how long to delay (if at all) before shutting down, or
  // repeating the playing
  Boolean timerIsBeingUsed = False;
  double secondsToDelay = duration;
  if (duration > 0) {
    timerIsBeingUsed = True;
    double absScale = scale > 0 ? scale : -scale; // ASSERT: scale != 0
    secondsToDelay = duration/absScale + durationSlop;

    int64_t uSecsToDelay = (int64_t)(secondsToDelay*1000000.0);
    sessionTimerTask = env->taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)sessionTimerHandler, (void*)NULL);
  }

  char const* actionString =  "Receiving streamed data";
  if (timerIsBeingUsed) {
    *env << actionString
		<< " (for up to " << secondsToDelay
		<< " seconds)...\n";
  } else {
    *env << actionString << "...\n";

  }

  /* finish timeout check */
  if(rtspTimeoutTask != NULL) {
      env->taskScheduler().unscheduleDelayedTask(rtspTimeoutTask);
      rtspTimeoutTask = NULL;
  }  

  // rtsp keep alive task 
  if(enableRtspKeepAlive == True) {
    hasReceiveKeepAliveResponse = True;
    rtspKeepAliveTask = env->taskScheduler().scheduleDelayedTask(RTSP_KEEPALIVE_INTERVAL, (TaskFunc*)sendRtspKeepAliveRequest, (void*)NULL);
  }




  // Watch for incoming packets (if desired):
  checkInterPacketGaps(NULL);
  
  /* callback user's function */
  if(userConstructCallback != NULL) 
  {
  	if (strcmp(g_pszCodecName, "H264") == 0) 
	{
		(*userConstructCallback)(RELAY_CLIENT_RESULT_SUCCESSFUL, outputSource, outputAudioSource);
        }
	else if (strcmp(g_pszCodecName, "MP4V-ES") == 0) 
	{
		(*userConstructCallback)(RELAY_CLIENT_RESULT_SUCCESSFUL, mpeg4Source, outputAudioSource);
	}
   	
  }
}



void sendRtspKeepAliveRequest(void* /*clientData*/)
{
    rtspKeepAliveTask = NULL;
    if(hasReceiveKeepAliveResponse == True) {
        
      if(session != NULL) {
          keepAliveSession(session, continueAfterKeepAlive);
          hasReceiveKeepAliveResponse = False;
      }



      /* need send again */
      rtspKeepAliveTask = env->taskScheduler().scheduleDelayedTask(RTSP_KEEPALIVE_INTERVAL, (TaskFunc*)sendRtspKeepAliveRequest, (void*)NULL);


    }

}


void continueAfterKeepAlive(RTSPClient*, int resultCode, char* resultString) 
{
    hasReceiveKeepAliveResponse = True;
}

void checkInterPacketGaps(void* /*clientData*/) {
  if (interPacketGapMaxTime == 0) return; // we're not checking

  // Check each subsession, counting up how many packets have been received:
  unsigned newTotNumPacketsReceived = 0;

  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;
    newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
  }

  if (newTotNumPacketsReceived == totNumPacketsReceived) {
    // No additional packets have been received since the last time we
    // checked, so end this stream:
    *env << "Closing session, because we stopped receiving packets.\n";
    interPacketGapCheckTimerTask = NULL;
    totNumPacketsReceived = ~0;
    RtspRtpError(RELAY_CLIENT_RESULT_INTER_PACKET_GAP);
  } else {
    totNumPacketsReceived = newTotNumPacketsReceived;
    // Check again, after the specified delay:
    interPacketGapCheckTimerTask
      = env->taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
				 (TaskFunc*)checkInterPacketGaps, NULL);
  }
}




/*---------------------------------------------------------------------------------------*/
/* qos */


class qosMeasurementRecord {
public:
  qosMeasurementRecord(struct timeval const& startTime, RTPSource* src, const char* mediaName)
    : fSource(src), fNext(NULL),
      kbits_per_second_min(1e20), kbits_per_second_max(0),
      kBytesTotal(0.0), kBytesTotalLast(0.0),
      packet_loss_fraction_min(1.0), packet_loss_fraction_max(0.0),
      totNumPacketsReceived(0), totNumPacketsExpected(0),
      totNumPacketsReceivedLast(0), totNumPacketsExpectedLast(0),
      nTotalFrameNumReceived(0),nIframeNumReceived(0),
      nTotalFrameNumReceivedLast(0), nIframeNumReceivedLast(0),
      nTotalFrameNumReceivedLastSecond(0)
      {
    measurementEndTime = measurementStartTime = startTime;

    memset(fMediaName, 0 , sizeof(fMediaName));
    if(mediaName != NULL) {
        strncpy(fMediaName,  mediaName, sizeof(fMediaName) - 1);
    }

  }
  virtual ~qosMeasurementRecord() { delete fNext; }

  void periodicQOSMeasurement(struct timeval const& timeNow);

  void QOSMeasurementReset(void);

public:
  RTPSource* fSource;
  qosMeasurementRecord* fNext;

public:
  struct timeval measurementStartTime, measurementEndTime;
  double kbits_per_second_min, kbits_per_second_max, kbits_per_second_now;
  double kBytesTotal, kBytesTotalLast;
  double packet_loss_fraction_min, packet_loss_fraction_max, packet_loss_fraction_now;
  unsigned totNumPacketsReceived, totNumPacketsExpected;
  unsigned totNumPacketsReceivedLast, totNumPacketsExpectedLast;
  unsigned nTotalFrameNumReceived, nIframeNumReceived;
  unsigned nTotalFrameNumReceivedLast, nIframeNumReceivedLast;
  unsigned nTotalFrameNumReceivedLastSecond;
  char fMediaName[64];
};

static qosMeasurementRecord* qosRecordHead = NULL;

static void periodicQOSMeasurement(void* clientData); // forward

static unsigned nextQOSMeasurementUSecs;

static void scheduleNextQOSMeasurement() {
  nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);
  unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
  unsigned usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;
     // Note: This works even when nextQOSMeasurementUSecs wraps around

  qosMeasurementTimerTask = env->taskScheduler().scheduleDelayedTask(
     usecsToDelay, (TaskFunc*)periodicQOSMeasurement, (void*)NULL);
}

static void periodicQOSMeasurement(void* /*clientData*/) {
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);

  for (qosMeasurementRecord* qosRecord = qosRecordHead;
       qosRecord != NULL; qosRecord = qosRecord->fNext) {
    qosRecord->periodicQOSMeasurement(timeNow);
  }

  // Do this again later:
  scheduleNextQOSMeasurement();
}

void qosMeasurementRecord
::periodicQOSMeasurement(struct timeval const& timeNow) {
  unsigned secsDiff = timeNow.tv_sec - measurementEndTime.tv_sec;
  int usecsDiff = timeNow.tv_usec - measurementEndTime.tv_usec;
  double timeDiff = secsDiff + usecsDiff/1000000.0;

  /* reset the qos data if needed */
  if(qosMeasurementResetIntervalSec > 0) {
    if(measurementEndTime.tv_sec - measurementStartTime.tv_sec >= qosMeasurementResetIntervalSec){
      QOSMeasurementReset();
    }
  }

  measurementEndTime = timeNow;

  RTPReceptionStatsDB::Iterator statsIter(fSource->receptionStatsDB());
  // Assume that there's only one SSRC source (usually the case):
  RTPReceptionStats* stats = statsIter.next(True);
  if (stats != NULL) {
    double kBytesTotalNow = stats->totNumKBytesReceived();
    double kBytesDeltaNow = kBytesTotalNow - kBytesTotal;
    kBytesTotal = kBytesTotalNow;

    double kbpsNow = timeDiff == 0.0 ? 0.0 : 8*kBytesDeltaNow/timeDiff;
    if (kbpsNow < 0.0) kbpsNow = 0.0; // in case of roundoff error
    if (kbpsNow < kbits_per_second_min) kbits_per_second_min = kbpsNow;
    if (kbpsNow > kbits_per_second_max) kbits_per_second_max = kbpsNow;
    kbits_per_second_now = kbpsNow;

    unsigned totReceivedNow = stats->totNumPacketsReceived();
    unsigned totExpectedNow = stats->totNumPacketsExpected();
    unsigned deltaReceivedNow = totReceivedNow - totNumPacketsReceived;
    unsigned deltaExpectedNow = totExpectedNow - totNumPacketsExpected;
    totNumPacketsReceived = totReceivedNow;
    totNumPacketsExpected = totExpectedNow;

    double lossFractionNow = deltaExpectedNow == 0 ? 0.0
      : 1.0 - deltaReceivedNow/(double)deltaExpectedNow;
    //if (lossFractionNow < 0.0) lossFractionNow = 0.0; //reordering can cause
    if (lossFractionNow < packet_loss_fraction_min) {
      packet_loss_fraction_min = lossFractionNow;
    }
    if (lossFractionNow > packet_loss_fraction_max) {
      packet_loss_fraction_max = lossFractionNow;
    }

    packet_loss_fraction_now = lossFractionNow;
    nTotalFrameNumReceivedLastSecond = nTotalFrameNumReceived;
    nTotalFrameNumReceived = g_nTotalFrameNum;
    nIframeNumReceived = g_nIframeNum;
#ifdef DEBUG
        fprintf(stderr, "g_nTotalFrameNum:%d\n", g_nTotalFrameNum);
	fprintf(stderr, "g_nIframeNum:%d\n", g_nIframeNum);
	fprintf(stderr, "g_nIntervalNumOfIframe:%d\n", g_nIntervalNumOfIframe);
#endif
  }
}

void qosMeasurementRecord
::QOSMeasurementReset(void) {

  measurementStartTime = measurementEndTime;
  kBytesTotalLast = kBytesTotal;
  totNumPacketsExpectedLast = totNumPacketsExpected;
  totNumPacketsReceivedLast = totNumPacketsReceived;
  kbits_per_second_max = 0;
  kbits_per_second_min = 1e20;
  packet_loss_fraction_max = 0;
  packet_loss_fraction_min = 1.0;
  nTotalFrameNumReceivedLast = nTotalFrameNumReceived;
  nIframeNumReceivedLast = nIframeNumReceived;

}

void beginQOSMeasurement() {
  // Set up a measurement record for each active subsession:
  struct timeval startTime;
  gettimeofday(&startTime, NULL);
  nextQOSMeasurementUSecs = startTime.tv_sec*1000000 + startTime.tv_usec;
  qosMeasurementRecord* qosRecordTail = NULL;
  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;

    /* only measure the video QOS by now */
    if (strcmp(subsession->mediumName(), "video") != 0) {
        continue;
    }

    qosMeasurementRecord* qosRecord
      = new qosMeasurementRecord(startTime, src, subsession->mediumName());
    if (qosRecordHead == NULL) qosRecordHead = qosRecord;
    if (qosRecordTail != NULL) qosRecordTail->fNext = qosRecord;
    qosRecordTail  = qosRecord;
  }

  // Then schedule the first of the periodic measurements:
  scheduleNextQOSMeasurement();
}

void printQOSData(int exitCode) {
  *env << "begin_QOS_statistics\n";
  
  // Print out stats for each active subsession:
  qosMeasurementRecord* curQOSRecord = qosRecordHead;
  if (session != NULL) {
    MediaSubsessionIterator iter(*session);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
      RTPSource* src = subsession->rtpSource();
      if (src == NULL) continue;
      
      *env << "subsession\t" << subsession->mediumName()
	   << "/" << subsession->codecName() << "\n";
      
      unsigned numPacketsReceived = 0, numPacketsExpected = 0;
      
      if (curQOSRecord != NULL) {
	numPacketsReceived = curQOSRecord->totNumPacketsReceived - curQOSRecord->totNumPacketsReceivedLast;
	numPacketsExpected = curQOSRecord->totNumPacketsExpected - curQOSRecord->totNumPacketsExpectedLast;
      }
      *env << "num_packets_received\t" << numPacketsReceived << "\n";
      *env << "num_packets_lost\t" << int(numPacketsExpected - numPacketsReceived) << "\n";
      
      if (curQOSRecord != NULL) {
	unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
	  - curQOSRecord->measurementStartTime.tv_sec;
	int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
	  - curQOSRecord->measurementStartTime.tv_usec;
	double measurementTime = secsDiff + usecsDiff/1000000.0;
	*env << "elapsed_measurement_time\t" << measurementTime << "\n";
	
	*env << "kBytes_received_total\t" 
         << (curQOSRecord->kBytesTotal - curQOSRecord->kBytesTotalLast) << "\n";
	
	*env << "measurement_sampling_interval_ms\t" << qosMeasurementIntervalMS << "\n";
	
	*env << "measurement_reset_interval_sec\t" << qosMeasurementResetIntervalSec << "\n";    
    	
	if (curQOSRecord->kbits_per_second_max == 0) {
	  // special case: we didn't receive any data:
	  *env <<
	    "kbits_per_second_min\tunavailable\n"
	    "kbits_per_second_ave\tunavailable\n"
	    "kbits_per_second_max\tunavailable\n";
	} else {
	  *env << "kbits_per_second_min\t" << curQOSRecord->kbits_per_second_min << "\n";
	  *env << "kbits_per_second_ave\t"
	       << (measurementTime == 0.0 ? 
               0.0 : 
               8*(curQOSRecord->kBytesTotal-curQOSRecord->kBytesTotalLast)/measurementTime) 
           << "\n";
	  *env << "kbits_per_second_max\t" << curQOSRecord->kbits_per_second_max << "\n";
	}
	
	*env << "packet_loss_percentage_min\t" << 100*curQOSRecord->packet_loss_fraction_min << "\n";
	double packetLossFraction = numPacketsExpected == 0 ? 1.0
	  : 1.0 - numPacketsReceived/(double)numPacketsExpected;
	if (packetLossFraction < 0.0) packetLossFraction = 0.0;
	*env << "packet_loss_percentage_ave\t" << 100*packetLossFraction << "\n";
	*env << "packet_loss_percentage_max\t"
	     << (packetLossFraction == 1.0 ? 100.0 : 100*curQOSRecord->packet_loss_fraction_max) << "\n";
	
	RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
	// Assume that there's only one SSRC source (usually the case):
	RTPReceptionStats* stats = statsIter.next(True);
	if (stats != NULL) {
	  *env << "inter_packet_gap_ms_min\t" << stats->minInterPacketGapUS()/1000.0 << "\n";
	  struct timeval totalGaps = stats->totalInterPacketGaps();
	  double totalGapsMS = totalGaps.tv_sec*1000.0 + totalGaps.tv_usec/1000.0;
	  unsigned totNumPacketsReceived = stats->totNumPacketsReceived();
	  *env << "inter_packet_gap_ms_ave\t"
	       << (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived) << "\n";
	  *env << "inter_packet_gap_ms_max\t" << stats->maxInterPacketGapUS()/1000.0 << "\n";
	}
	
	curQOSRecord = curQOSRecord->fNext;
      }
    }
  }

  *env << "end_QOS_statistics\n";
}




void endQOSMeasurement() {


/* for debug */
#if 0


  *env << "begin_QOS_statistics\n";
  
  // Print out stats for each active subsession:
  qosMeasurementRecord* curQOSRecord = qosRecordHead;
  if (session != NULL) {
    MediaSubsessionIterator iter(*session);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
      RTPSource* src = subsession->rtpSource();
      if (src == NULL) continue;
      
      *env << "subsession\t" << subsession->mediumName()
	   << "/" << subsession->codecName() << "\n";
      
      unsigned numPacketsReceived = 0, numPacketsExpected = 0;
      
      if (curQOSRecord != NULL) {
	numPacketsReceived = curQOSRecord->totNumPacketsReceived;
	numPacketsExpected = curQOSRecord->totNumPacketsExpected;
      }
      *env << "num_packets_received\t" << numPacketsReceived << "\n";
      *env << "num_packets_lost\t" << int(numPacketsExpected - numPacketsReceived) << "\n";
      
      if (curQOSRecord != NULL) {
	unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
	  - curQOSRecord->measurementStartTime.tv_sec;
	int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
	  - curQOSRecord->measurementStartTime.tv_usec;
	double measurementTime = secsDiff + usecsDiff/1000000.0;
	*env << "elapsed_measurement_time\t" << measurementTime << "\n";
	
	*env << "kBytes_received_total\t" << curQOSRecord->kBytesTotal << "\n";
	
	*env << "measurement_sampling_interval_ms\t" << qosMeasurementIntervalMS << "\n";
	
	if (curQOSRecord->kbits_per_second_max == 0) {
	  // special case: we didn't receive any data:
	  *env <<
	    "kbits_per_second_min\tunavailable\n"
	    "kbits_per_second_ave\tunavailable\n"
	    "kbits_per_second_max\tunavailable\n";
	} else {
	  *env << "kbits_per_second_min\t" << curQOSRecord->kbits_per_second_min << "\n";
	  *env << "kbits_per_second_ave\t"
	       << (measurementTime == 0.0 ? 0.0 : 8*curQOSRecord->kBytesTotal/measurementTime) << "\n";
	  *env << "kbits_per_second_max\t" << curQOSRecord->kbits_per_second_max << "\n";
	}
	
	*env << "packet_loss_percentage_min\t" << 100*curQOSRecord->packet_loss_fraction_min << "\n";
	double packetLossFraction = numPacketsExpected == 0 ? 1.0
	  : 1.0 - numPacketsReceived/(double)numPacketsExpected;
	if (packetLossFraction < 0.0) packetLossFraction = 0.0;
	*env << "packet_loss_percentage_ave\t" << 100*packetLossFraction << "\n";
	*env << "packet_loss_percentage_max\t"
	     << (packetLossFraction == 1.0 ? 100.0 : 100*curQOSRecord->packet_loss_fraction_max) << "\n";
	
	RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
	// Assume that there's only one SSRC source (usually the case):
	RTPReceptionStats* stats = statsIter.next(True);
	if (stats != NULL) {
	  *env << "inter_packet_gap_ms_min\t" << stats->minInterPacketGapUS()/1000.0 << "\n";
	  struct timeval totalGaps = stats->totalInterPacketGaps();
	  double totalGapsMS = totalGaps.tv_sec*1000.0 + totalGaps.tv_usec/1000.0;
	  unsigned totNumPacketsReceived = stats->totNumPacketsReceived();
	  *env << "inter_packet_gap_ms_ave\t"
	       << (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived) << "\n";
	  *env << "inter_packet_gap_ms_max\t" << stats->maxInterPacketGapUS()/1000.0 << "\n";
	}
	
	curQOSRecord = curQOSRecord->fNext;
      }
    }
  }

  *env << "end_QOS_statistics\n";
#endif

    if(qosRecordHead != NULL) {
      delete qosRecordHead;
      qosRecordHead = NULL;
    }
}

int getQOSVideoData(unsigned  *outMeasurementTime,
                    unsigned  *outNumPacketsReceived,
                    unsigned  *outNumPacketsExpected, 
                    unsigned  *outKbitsPerSecondMin,
                    unsigned  *outKbitsPerSecondMax,
                    unsigned  *outKbitsPerSecondAvg,
                    unsigned  *outKbitsPerSecondNow,
                    unsigned  *outPacketLossPercentageMin,
                    unsigned  *outPacketLossPercentageMax,
                    unsigned  *outPacketLossPercentageAvg, 
                    unsigned  *outPacketLossPercentageNow,
                    unsigned  *outFpsAvg,
                    unsigned  *outFpsNow,
                    unsigned  *outGovAvg,
                    unsigned  *outGovNow
                    )
{

    /* santiy check */
    if(outMeasurementTime == NULL || outNumPacketsExpected == NULL ||
       outNumPacketsReceived == NULL || outKbitsPerSecondMin == NULL ||
       outKbitsPerSecondMax == NULL || outKbitsPerSecondAvg == NULL ||
       outKbitsPerSecondNow == NULL ||
       outPacketLossPercentageMin == NULL || outPacketLossPercentageMax == NULL ||
       outPacketLossPercentageAvg == NULL || outPacketLossPercentageNow == NULL ||
       NULL == outFpsAvg ||NULL == outFpsNow ||NULL == outGovAvg ||NULL == outGovNow
      ) 
    {
        
        return -1;
    }
    
    if(qosRecordHead == NULL) {
        return -2; /* no qos enabled */
    }


    // Print out stats for each active subsession:
    qosMeasurementRecord* curQOSRecord = qosRecordHead;

    while (curQOSRecord != NULL) {

        if (strcmp(curQOSRecord->fMediaName, "video") != 0) {
            /* next record */
            curQOSRecord = curQOSRecord->fNext;
            continue;
        }


        /* measure time */
        unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
            - curQOSRecord->measurementStartTime.tv_sec;
        int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
            - curQOSRecord->measurementStartTime.tv_usec;
        double measurementTime = secsDiff + usecsDiff/1000000.0;
        
        *outMeasurementTime = (unsigned) measurementTime;


        /* packet statistic */
        *outNumPacketsReceived = curQOSRecord->totNumPacketsReceived - curQOSRecord->totNumPacketsReceivedLast;
        *outNumPacketsExpected = curQOSRecord->totNumPacketsExpected - curQOSRecord->totNumPacketsExpectedLast;


        /* throughput statistic */
        *outKbitsPerSecondMin = (unsigned) curQOSRecord->kbits_per_second_min;
        *outKbitsPerSecondMax = (unsigned) curQOSRecord->kbits_per_second_max;
        *outKbitsPerSecondAvg = (measurementTime == 0.0 ? 
                                            (unsigned)0 : 
               (unsigned) (8*(curQOSRecord->kBytesTotal-curQOSRecord->kBytesTotalLast)/measurementTime) );
        *outKbitsPerSecondNow = (unsigned) curQOSRecord->kbits_per_second_now;

        /* packet loss statistic */
        *outPacketLossPercentageMin = (unsigned) (100*curQOSRecord->packet_loss_fraction_min);
        *outPacketLossPercentageMax = (unsigned) (100*curQOSRecord->packet_loss_fraction_max);
        *outPacketLossPercentageNow = (unsigned)  (100*curQOSRecord->packet_loss_fraction_now);
        double packetLossFraction = (*outNumPacketsExpected) == 0 ? 1.0
            : 1.0 - (double)(*outNumPacketsReceived)/(double)(*outNumPacketsExpected);
        if (packetLossFraction < 0.0) packetLossFraction = 0.0;
        *outPacketLossPercentageAvg = (unsigned) (100*packetLossFraction);
	
	/* frame statistic */
	*outFpsAvg = (measurementTime == 0.0 ? (unsigned)0 : 
               (unsigned) (100*(curQOSRecord->nTotalFrameNumReceived-curQOSRecord->nTotalFrameNumReceivedLast)/measurementTime) );
	double nInterval = (double)(qosMeasurementIntervalMS) /1000.0;
	*outFpsNow = (nInterval == 0.0 ? (unsigned)0 : 
               (unsigned) (100*(curQOSRecord->nTotalFrameNumReceived-curQOSRecord->nTotalFrameNumReceivedLastSecond)/nInterval) );
	unsigned nIframeNumPerTime = (unsigned) (curQOSRecord->nIframeNumReceived-curQOSRecord->nIframeNumReceivedLast);
	unsigned nTotalFrameNumPerTime = (unsigned) (curQOSRecord->nTotalFrameNumReceived-curQOSRecord->nTotalFrameNumReceivedLast);
	*outGovAvg = (nIframeNumPerTime == 0 ? (unsigned)0 :  (unsigned) (100*nTotalFrameNumPerTime/nIframeNumPerTime) );
	*outGovNow = 100*g_nIntervalNumOfIframe;
        /* next record */
        curQOSRecord = curQOSRecord->fNext;

    }

    return 0;


}




/*---------------------------------------------------------------------------------------*/
/* shutdown */


void closeMediaSinks() {

/*
	Medium::close(qtOut);
  Medium::close(aviOut);
*/
  if (session == NULL) return;
  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    Medium::close(subsession->sink);
    subsession->sink = NULL;
  }
}

void closeLiveVideoFramer() 
{

  if(outputSource != NULL) {

	/* detach the live framer with the underlying souce */
    outputSource->detachInputSource();
		
		
	/* issue a onClose notification on this source */
    FramedSource::handleClosure(outputSource);
  
    /* close it */
    Medium::close(outputSource);

    outputSource = NULL;
  }
  if(mpeg4Source != NULL) {

	/* detach the live framer with the underlying souce */
    mpeg4Source->detachInputSource();
		
		
	/* issue a onClose notification on this source */
    FramedSource::handleClosure(mpeg4Source);
  
    /* close it */
    Medium::close(mpeg4Source);

    mpeg4Source = NULL;
  }

}
void closeAudioSource()
{
  if(outputAudioSource != NULL) {
		
	/* issue a onClose notification on this source */
    FramedSource::handleClosure(outputAudioSource);

    outputAudioSource = NULL;

    /* don't close it, because it would be closed when the session is closed */

  }
}

void subsessionByeHandler(void* clientData) {
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);
  unsigned secsDiff = timeNow.tv_sec - startTime.tv_sec;

  MediaSubsession* subsession = (MediaSubsession*)clientData;
  *env << "Received RTCP \"BYE\" on \"" << subsession->mediumName()
	<< "/" << subsession->codecName()
	<< "\" subsession (after " << secsDiff
	<< " seconds)\n";

  // Act now as if the subsession had closed:
  RtspRtpError(RELAY_CLIENT_RESULT_BYE);
}

void sessionTimerHandler(void* /*clientData*/) {
  sessionTimerTask = NULL;
  if (env != NULL) {  
    *env << "Closing session, because we timer timeout.\n";
  }
  
  RtspRtpError(RELAY_CLIENT_RESULT_SESSION_TIMER);
}




static int shutdownErrorType;

void continueAfterTEARDOWN(RTSPClient*, int /*resultCode*/, char* /*resultString*/) {

	
	if (env != NULL && tearDownTask != NULL) {
        env->taskScheduler().unscheduleDelayedTask(tearDownTask);
		tearDownTask = NULL;
	}
		
	// Now that we've stopped any more incoming data from arriving, close our output files:

	closeLiveVideoFramer();
    closeAudioSource();

	closeMediaSinks();

	Medium::close(session);
    session = NULL;

	// Finally, shut down our client:
	if (ourAuthenticator != NULL) {
		delete ourAuthenticator;
		ourAuthenticator = NULL;
	}
	Medium::close(ourClient);
    ourClient = NULL;


	/* call back user's function */
	if(isRTSPConstructFail) {
      if(userConstructCallback != NULL) {
        (*userConstructCallback)(shutdownErrorType, NULL, NULL);
      }
	}else{
      if(userErrorCallback != NULL) {
        (*userErrorCallback)(shutdownErrorType);
      }
	}
	

}


void tearDownTimeout(void * dummy)
{
  tearDownTask = NULL;
  continueAfterTEARDOWN(NULL, 0, NULL);
}


void clientShutdownRoutine(void * dummy)
{
  // Teardown, then shutdown, any outstanding RTP/RTCP subsessions
  if (session != NULL) {
    /*delay 1 second to finish teardown*/
	  tearDownTask = 
      env->taskScheduler().scheduleDelayedTask(1000000, (TaskFunc*)tearDownTimeout, (void*)NULL);

    tearDownSession(session, continueAfterTEARDOWN);
  } else {
    continueAfterTEARDOWN(NULL, 0, NULL);
  }

}


void clientShutdown(int errorType) {
  if (areAlreadyShuttingDown) return; // in case we're called after receiving a RTCP "BYE" while in the middle of a "TEARDOWN".
  areAlreadyShuttingDown = True;


  if (qosMeasurementIntervalMS > 0) {
    endQOSMeasurement();
  }

  shutdownErrorType = errorType;
  if (env != NULL) {
    if(sessionTimerTask != NULL) {
        env->taskScheduler().unscheduleDelayedTask(sessionTimerTask);
    }
    if(interPacketGapCheckTimerTask != NULL) {
        env->taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
    }
    if(qosMeasurementTimerTask != NULL) {
        env->taskScheduler().unscheduleDelayedTask(qosMeasurementTimerTask);
    }
    if(rtspKeepAliveTask != NULL) {
        env->taskScheduler().unscheduleDelayedTask(rtspKeepAliveTask);
    }    
    if(rtspTimeoutTask != NULL) {
        env->taskScheduler().unscheduleDelayedTask(rtspTimeoutTask);
    }    
    		
	sessionTimerTask = NULL;
	interPacketGapCheckTimerTask = NULL;
    qosMeasurementTimerTask = NULL;
    rtspKeepAliveTask = NULL;
    totNumPacketsReceived = ~0;
    rtspTimeoutTask = NULL;


	/* avoid the deep stack invoke*/
    (void)env->taskScheduler().scheduleDelayedTask(0,
  	    			          (TaskFunc*)clientShutdownRoutine, NULL);
  }



}


void RTSPConstructFail(int errorType) {
	
	isRTSPConstructFail = True;
	clientShutdown(errorType);/* shut down the rtsp client */
}


void RtspRtpError(int errorType) {
	
	isRTSPConstructFail = False;
	clientShutdown(errorType);/* shut down the rtsp client */
}

void shutdownRtspClient(void)
{
  RtspRtpError(RELAY_CLIENT_RESULT_USER_DEMAND);
}
#endif 