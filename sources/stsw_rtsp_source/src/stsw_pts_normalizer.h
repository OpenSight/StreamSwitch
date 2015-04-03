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
 * stsw_pts_normalizer.h
 *      PtsSessionNormalizer & PtsSubsessionNormalizer class header file, 
 * Declare all the interface of the both class
 * 
 * author: jamken
 * date: 2015-4-1
**/ 


#ifndef STSW_PTS_NORMALIZER_H
#define STSW_PTS_NORMALIZER_H

#ifndef STREAM_SWITCH
#define STREAM_SWITCH
#endif

#include "liveMedia.hh"

////////// PtsSessionNormalizer and PtsSubsessionNormalizer definitions //////////

// The following two classes are used by rtsp client to convert incoming streams' 
// presentation times into local wall-clock-aligned presentation times that are suitable 
// for our stream-switch source 
//(for the corresponding outgoing frames).
// (For multi-subsession (i.e., audio+video) sessions, the outgoing streams' presentation times 
//retain the same relative  separation as those of the incoming streams.)
class PtsSessionNormalizer;

class PtsSubsessionNormalizer: public FramedFilter {
public:    
    char const* mediumName() const { return fMediumName; }
    char const* codecName() const { return fCodecName; }  
  
private:
    friend class PtsSessionNormalizer;
    PtsSubsessionNormalizer(PtsSessionNormalizer& parent, FramedSource* inputSource, RTPSource* rtpSource,
				       char const *mediumName, char const* codecName, PtsSubsessionNormalizer* next);
      // called only from within "PtsSessionNormalizer"
    virtual ~PtsSubsessionNormalizer();

    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
    void afterGettingFrame(unsigned frameSize,
                unsigned numTruncatedBytes,
                struct timeval presentationTime,
                unsigned durationInMicroseconds);

private: // redefined virtual functions:
    virtual void doGetNextFrame();

private:
    PtsSessionNormalizer& fParent;
    RTPSource* fRTPSource;
    char * fCodecName;
    char * fMediumName; 
    u_int32_t fLastRtpTimestamp;
    PtsSubsessionNormalizer* fNext;
};

class PtsSessionNormalizer: public Medium {
public:
    PtsSessionNormalizer(UsageEnvironment& env);
    virtual ~PtsSessionNormalizer();

    PtsSubsessionNormalizer* createNewPtsSubsessionNormalizer(
        FramedSource* inputSource, RTPSource* rtpSource, 
        char const *mediumName, char const* codecName);

private: // called only from within "~PtsSubsessionNormalizer":
    friend class PtsSubsessionNormalizer;
    void normalizePresentationTime(PtsSubsessionNormalizer* ssNormalizer,
				 struct timeval const& fromPT, struct timeval * toPT);
    void removePresentationTimeSubsessionNormalizer(PtsSubsessionNormalizer* ssNormalizer);

private:
    PtsSubsessionNormalizer* fSubsessionNormalizers;
  
    // used for subsessions that have been RTCP-synced, prefer vedio
    PtsSubsessionNormalizer* fMasterSSNormalizer; 

    struct timeval fPTAdjustment; // Added to (RTCP-synced) subsession presentation times to 'normalize' them with wall-clock time.
};

#endif
