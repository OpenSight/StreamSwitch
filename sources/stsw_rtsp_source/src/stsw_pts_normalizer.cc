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
 * stsw_pts_normalizer.cc
 *      PtsSessionNormalizer & PtsSubsessionNormalizer class implementation file, 
 * define all the methods of the both class
 * 
 * author: jamken
 * date: 2015-4-1
**/ 

#include "stsw_pts_normalizer.h"


#include <sys/time.h>

#ifndef MILLION
#define MILLION 1000000
#endif


////////// PtsSessionNormalizer and PtsSubsessionNormalizer implementations //////////

// PtsSessionNormalizer:

PtsSessionNormalizer::PtsSessionNormalizer(UsageEnvironment& env)
  : Medium(env),
    fSubsessionNormalizers(NULL), fMasterSSNormalizer(NULL) {
}

PtsSessionNormalizer::~PtsSessionNormalizer() {
  while (fSubsessionNormalizers != NULL) {
    Medium::close(fSubsessionNormalizers);
  }
}

PtsSubsessionNormalizer*
PtsSessionNormalizer::createNewPtsSubsessionNormalizer(FramedSource* inputSource, RTPSource* rtpSource,
										 char const *mediumName, char const* codecName) {
  fSubsessionNormalizers
    = new PtsSubsessionNormalizer(*this, inputSource, rtpSource, mediumName, codecName, fSubsessionNormalizers);
  return fSubsessionNormalizers;
}

void PtsSessionNormalizer::normalizePresentationTime(PtsSubsessionNormalizer* ssNormalizer,
								  struct timeval const& fromPT, struct timeval * toPT) {
    Boolean const hasBeenSynced = ssNormalizer->fRTPSource->hasBeenSynchronizedUsingRTCP();

    if (!hasBeenSynced) {
        // If "fromPT" has not yet been RTCP-synchronized, then it was generated 
        // by our own receiving code, and thus
        // is already aligned with 'wall-clock' time.  
        // Just copy it 'as is' to "toPT":
        *toPT = fromPT;
    } else {
      
        if (fMasterSSNormalizer == NULL){
            // Make "ssNormalizer" the 'master' subsession - meaning that its presentation time is adjusted to align with 'wall clock'
            // time, and the presentation times of other subsessions (if any) are adjusted to retain their relative separation with
            // those of the master:
      
            struct timeval timeNow;
            gettimeofday(&timeNow, NULL);
            
            //toPT now is the PT of the last packet 
            //if now time is less than the last packet PT, 
            //use the last packet PT at the now time to avoid
            //timestamp rollback
            if((timeNow.tv_sec < toPT.tv_sec) ||
               (timeNow.tv_sec == toPT.tv_sec && 
                timeNow.tv_usec < toPT.tv_usec )){
                
                timeNow.tv_sec = toPT.tv_sec;
                timeNow.tv_usec = toPT.tv_usec;
            }

            fMasterSSNormalizer = ssNormalizer;
            // Compute: fPTAdjustment = timeNow - fromPT
            fPTAdjustment.tv_sec = timeNow.tv_sec - fromPT.tv_sec;
            fPTAdjustment.tv_usec = timeNow.tv_usec - fromPT.tv_usec;
            // Note: It's OK if one or both of these fields underflows; 
            // the result still works out OK later.
        }

        // Compute a normalized presentation time: toPT = fromPT + fPTAdjustment
        toPT->tv_sec = fromPT.tv_sec + fPTAdjustment.tv_sec - 1;
        toPT->tv_usec = fromPT.tv_usec + fPTAdjustment.tv_usec + MILLION;
        while (toPT->tv_usec > MILLION) { 
            ++(toPT->tv_sec); toPT->tv_usec -= MILLION; 
        }

    }//if (!hasBeenSynced) {
}

void PtsSessionNormalizer
::removePresentationTimeSubsessionNormalizer(PtsSubsessionNormalizer* ssNormalizer) {
  // Unlink "ssNormalizer" from the linked list (starting with "fSubsessionNormalizers"):
  if (fSubsessionNormalizers == ssNormalizer) {
    fSubsessionNormalizers = fSubsessionNormalizers->fNext;
  } else {
    PtsSubsessionNormalizer** ssPtrPtr = &(fSubsessionNormalizers->fNext);
    while (*ssPtrPtr != ssNormalizer) ssPtrPtr = &((*ssPtrPtr)->fNext);
    *ssPtrPtr = (*ssPtrPtr)->fNext;
  }
  if(fMasterSSNormalizer == ssNormalizer){
      fMasterSSNormalizer = NULL;
  }
}

// PtsSubsessionNormalizer:

PtsSubsessionNormalizer
::PtsSubsessionNormalizer(PtsSessionNormalizer& parent, FramedSource* inputSource, RTPSource* rtpSource,
				       char const *mediumName, char const* codecName, PtsSubsessionNormalizer* next)
  : FramedFilter(parent.envir(), inputSource),
    fParent(parent), fRTPSource(rtpSource), 
    fCodecName(NULL), fMediumName(NULL), fNext(next) {
    if(codecName != NULL){
        fCodecName = strdup(codecName);        
    }
    if(mediumName != NULL){
        fMediumName = strdup(mediumName);
    }

}

PtsSubsessionNormalizer::~PtsSubsessionNormalizer() {
  fParent.removePresentationTimeSubsessionNormalizer(this);
  
    if(fCodecName != NULL){
        free(fCodecName); 
        fCodecName = NULL;
        
    }
    if(fMediumName != NULL){
        free(fMediumName);
        fMediumName = NULL;
    }  
}

void PtsSubsessionNormalizer::afterGettingFrame(void* clientData, unsigned frameSize,
							     unsigned numTruncatedBytes,
							     struct timeval presentationTime,
							     unsigned durationInMicroseconds) {
  ((PtsSubsessionNormalizer*)clientData)
    ->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void PtsSubsessionNormalizer::afterGettingFrame(unsigned frameSize,
							     unsigned numTruncatedBytes,
							     struct timeval presentationTime,
							     unsigned durationInMicroseconds) {
  // This filter is implemented by passing all frames through unchanged, except that "fPresentationTime" is changed:
  fFrameSize = frameSize;
  fNumTruncatedBytes = numTruncatedBytes;
  fDurationInMicroseconds = durationInMicroseconds;

  fParent.normalizePresentationTime(this, presentationTime, &fPresentationTime);
   
  // Complete delivery:
  FramedSource::afterGetting(this);
}

void PtsSubsessionNormalizer::doGetNextFrame() {
  fInputSource->getNextFrame(fTo, fMaxSize, afterGettingFrame, this, FramedSource::handleClosure, this);
}


