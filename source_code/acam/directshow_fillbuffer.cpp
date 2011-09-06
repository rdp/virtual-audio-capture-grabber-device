#include "stdafx.h"
#include "acam.h"


CCritSec m_cSharedState;

//
// FillBuffer
//
// Stuffs the buffer with data
// "they" call this, every so often...
HRESULT CVCamStream::FillBuffer(IMediaSample *pms) 
{	
	// I don't expect these...the parent controls this/us and doesn't call us when it is stopped I guess, so should be active
	assert(m_pParent->IsActive());
	assert(!m_pParent->IsStopped());

    CheckPointer(pms,E_POINTER);
    BYTE *pData;
    HRESULT hr = pms->GetPointer(&pData);
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    
	LONG totalWrote = -1;
	hr = LoopbackCaptureTakeFromBuffer(pData, pms->GetSize(), NULL, &totalWrote); // the real meat
	if(FAILED(hr)) {
		// this one can return false during shutdown, so it's actually ok to just return from here...
		// assert(false);
		return hr;
	}

	CAutoLock cAutoLockShared(&m_cSharedState); // for the bFirstPacket boolean control, except there's probably still some odd race condition er other...

	hr = pms->SetActualDataLength(totalWrote);
	if(FAILED(hr)) {
		assert(false);
		return hr;
	}
	WAVEFORMATEX* pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

    // Set the sample's start and end time stamps...
    CRefTime rtStart;
	if(bFirstPacket) {
      m_pParent->StreamTime(rtStart); // get current graph ref time as "start", as normal "capture" devices would
	} else {
		// since there hasn't been discontinuity, I think we should be safe to tell it
		// that this packet starts where the previous packet ended off
		// since that's theoretically accurate...
    	REFERENCE_TIME previousEnd = m_rtSampleEndTime;
		rtStart = previousEnd;
		bFirstPacket = false;
	}


	// I once tried to change it to always have monotonicity of timestamps at this point, but it didn't fix any problems, and seems to do all right without it [?]

    m_rtSampleEndTime = rtStart + (REFERENCE_TIME)(UNITS * pms->GetActualDataLength()) / 
                     (REFERENCE_TIME)pwfexCurrent->nAvgBytesPerSec;

	// NB that this *can* set it negative...odd...hmm...
    hr = pms->SetTime((REFERENCE_TIME*)&rtStart, (REFERENCE_TIME*)&m_rtSampleEndTime);
	if (FAILED(hr)) {
		assert(false);
        return hr;
    }
	// if we do SetTime(NULL, NULL) here then VLC can "play" it with directshows buffers of size 0ms.
	// however, then VLC cannot then stream it at all.  So we leave it set to some time, and just require you to have VLC buffers of at least 40 or 50 ms
	// [a possible VLC bug?] http://forum.videolan.org/viewtopic.php?f=14&t=92659&hilit=+50ms

    hr = pms->SetMediaTime((REFERENCE_TIME*)&rtStart, (REFERENCE_TIME*)&m_rtSampleEndTime);
    m_llSampleMediaTimeStart = m_rtSampleEndTime;

	if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    // Set the sample's properties.
    hr = pms->SetPreroll(FALSE); // tell it that this isn't preroll, so to actually use it
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    hr = pms->SetMediaType(NULL);
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }
   
    hr = pms->SetDiscontinuity(!bFirstPacket);
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }
    
    hr = pms->SetSyncPoint(!bFirstPacket);
	if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    return NOERROR;

} // end FillBuffer
