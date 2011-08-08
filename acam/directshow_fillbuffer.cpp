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

	assert(m_pParent->IsActive()); // wouldn't expect these...
	assert(!m_pParent->IsStopped());
    CheckPointer(pms,E_POINTER);
    BYTE *pData;
    HRESULT hr = pms->GetPointer(&pData);
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    CAutoLock cAutoLockShared(&m_cSharedState); // do we have any threading conflicts though? don't think so...just in case though :P

	LONG totalWrote = -1;
	hr = LoopbackCaptureTakeFromBuffer(pData, pms->GetSize(), NULL, &totalWrote); // the real meat
	if(FAILED(hr)) {
		// this one can return false during shutdown, so ok to return from here...
		// assert(false);
		return hr;
	}

	hr = pms->SetActualDataLength(totalWrote);
	if(FAILED(hr)) {
		assert(false);
		return hr;
	}
	WAVEFORMATEX* pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

    // Set the sample's start and end time stamps...
    CRefTime rtStart;
    m_pParent->StreamTime(rtStart);
	REFERENCE_TIME previousEnd = m_rtSampleEndTime;

	// I once tried to change it to always have monotonicity of timestamps at this point, but it didn't fix any problems, and seems to do all right without it [?]

    m_rtSampleEndTime = rtStart + (REFERENCE_TIME)(UNITS * pms->GetActualDataLength()) / 
                     (REFERENCE_TIME)pwfexCurrent->nAvgBytesPerSec;

	// NB that this *can* set it negative...odd...hmm...
    hr = pms->SetTime((REFERENCE_TIME*)&rtStart, (REFERENCE_TIME*)&m_rtSampleEndTime);

	// if we do SetTime(NULL, NULL) here then VLC can "play" it with directshows buffers of size 0ms.
	// however, then VLC cannot then stream it at all.  So we leave it set to some time, and just require you to have VLC buffers of at least 40 or 50 ms
	// [a possible VLC bug?] http://forum.videolan.org/viewtopic.php?f=14&t=92659&hilit=+50ms
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
    bFirstPacket = false; // we're feeding it now...

	// MediaTime is our own internal "offset" so fill it in starting from 0...http://msdn.microsoft.com/en-us/library/aa451394.aspx
	// which actually might work for us :)
    LONGLONG llMediaTimeStart = m_llSampleMediaTimeStart;
    DWORD dwNumAudioSamplesInPacket = (pms->GetActualDataLength() * BITS_PER_BYTE) /
                                      (pwfexCurrent->nChannels * pwfexCurrent->wBitsPerSample);
    LONGLONG llMediaTimeStop = m_llSampleMediaTimeStart + dwNumAudioSamplesInPacket;
    hr = pms->SetMediaTime(&llMediaTimeStart, &llMediaTimeStop);
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    m_llSampleMediaTimeStart = llMediaTimeStop;
    return NOERROR;
} // end FillBuffer
