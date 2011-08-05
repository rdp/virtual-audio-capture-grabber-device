#include "stdafx.h"
#include "acam.h"


extern IReferenceClock *globalClock;

//
// FillBuffer
//
// Stuffs the buffer with data
// "they" call this, every so often...
// you probably should fill the entire buffer...I think...hmm...
HRESULT CVCamStream::FillBuffer(IMediaSample *pms) 
{
	
    // If graph is inactive stop cueing samples
	// hmm ... not sure if this can ever be the case...

	assert(m_pParent->IsActive());
	assert(!m_pParent->IsStopped());
    CheckPointer(pms,E_POINTER);
    BYTE *pData;
    HRESULT hr = pms->GetPointer(&pData);
    if (FAILED(hr)) {
        return hr;
    }

	LONG totalWrote = -1;
	hr = LoopbackCaptureTakeFromBuffer(pData, pms->GetSize(), NULL, &totalWrote); // the real meat
	if(FAILED(hr)) {
		return hr;
	}

	hr = pms->SetActualDataLength(totalWrote);
	if(FAILED(hr)) {
		return hr;
	}

	WAVEFORMATEX* pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();


	IReferenceClock* pClock;
	m_pParent->GetSyncSource(&pClock); // I could do this an easier way...https://github.com/rdp/open-source-directshow-video-capture-demo-filter/commit/71fd443e3b28a8e43210cc3cc48b05b63273b5b7
	REFERENCE_TIME now;
	pClock->GetTime(&now);
	REFERENCE_TIME previousEnd = m_rtSampleEndTime;
	pClock->Release();

    // Set the sample's start and end time stamps...
	if(now < latestGraphStart) {
	  // maybe we're just the freaky too early off-chance?
      // assert(now > latestGraphStart);
		ShowOutput("too early? %d %d ", now, latestGraphStart);
	}
    REFERENCE_TIME rtStart = now - latestGraphStart; // this is the same as m_pParent->StreamTime(rtStart2);
	if(!(rtStart >= m_rtSampleEndTime)) {
	  //assert(rtStart >= m_rtSampleEndTime);
	  //rtStart = m_rtSampleEndTime; // ??? doesn't fix it...hmm...
	} else {
      m_rtSampleEndTime = rtStart + (REFERENCE_TIME)(UNITS * pms->GetActualDataLength()) / 
                     (REFERENCE_TIME)pwfexCurrent->nAvgBytesPerSec;
	}

	if(!(rtStart <= m_rtSampleEndTime)) {
	  assert(rtStart <= m_rtSampleEndTime); // sanity check my math...
	}

    hr = pms->SetTime((REFERENCE_TIME*)&rtStart, (REFERENCE_TIME*)&m_rtSampleEndTime);
    //hr = pms->SetTime(NULL, NULL); // causes it to not stream right in VLC...though
	// also setting any time at all seems to cause VLC, if you set directshow buffers to none, to not
	// play the audio, so, we lose both ways, but this way works at least if you have a 50ms buffer.
	if (FAILED(hr)) {
        return hr;
    }

    // Set the sample's properties.

    hr = pms->SetPreroll(FALSE); // tell it that this isn't preroll, so actually use it
    if (FAILED(hr)) {
        return hr;
    }

    hr = pms->SetMediaType(NULL);
    if (FAILED(hr)) {
        return hr;
    }
   
    hr = pms->SetDiscontinuity(!bFirstPacket);
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = pms->SetSyncPoint(!bFirstPacket);
	if (FAILED(hr)) {
        return hr;
    }

	// MediaTime is our own internal "offset" so fill it in starting from 0...http://msdn.microsoft.com/en-us/library/aa451394.aspx
	// which actually might work for us :)
    LONGLONG llMediaTimeStart = m_llSampleMediaTimeStart;
    DWORD dwNumAudioSamplesInPacket = (pms->GetActualDataLength() * BITS_PER_BYTE) /
                                      (pwfexCurrent->nChannels * pwfexCurrent->wBitsPerSample);
    LONGLONG llMediaTimeStop = m_llSampleMediaTimeStart + dwNumAudioSamplesInPacket;
    hr = pms->SetMediaTime(&llMediaTimeStart, &llMediaTimeStop);
    if (FAILED(hr)) {
        return hr;
    }

    m_llSampleMediaTimeStart = llMediaTimeStop;
    return NOERROR;
} // end FillBuffer
