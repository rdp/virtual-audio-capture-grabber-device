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
	// I don't expect these...the parent controls this/us and doesn't call us when it is stopped I guess, so we should always be active...
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
	// the real meat -- get all the incoming data
	hr = LoopbackCaptureTakeFromBuffer(pData, pms->GetSize(), NULL, &totalWrote);
	if(FAILED(hr)) {
		// this one can return false during shutdown, so it's actually ok to just return from here...
		// assert(false);
		return hr;
	}

	CAutoLock cAutoLockShared(&m_cSharedState); // for the bFirstPacket boolean control, except there's probably still some odd race conditions er other...

	hr = pms->SetActualDataLength(totalWrote);
	if(FAILED(hr)) {
		assert(false);
		return hr;
	}

    // Now set the sample's start and end time stamps...

	WAVEFORMATEX* pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();
	CRefTime sampleTimeUsed = (REFERENCE_TIME)(UNITS * pms->GetActualDataLength()) / 
                     (REFERENCE_TIME)pwfexCurrent->nAvgBytesPerSec;
    CRefTime rtStart;
	if(bFirstPacket) { // bFirstPacket or true here...true seemed to help that one guy...
      m_pParent->StreamTime(rtStart); // gets current graph ref time [now] as its "start", as normal "capture" devices would, just in case that's better...
	  if(bFirstPacket)
	    ShowOutput("retrieving a first packet");
	} else {
		// since there hasn't been discontinuity, I think we should be safe to tell it
		// that this packet starts where the previous packet ended off
		// since that's theoretically accurate...
		// exept that it ends up being bad [?]
		// I don't "think" this will hurt graphs that have no reference clock...hopefully...

		rtStart = m_rtPreviousSampleEndTime;

        // CRefTime cur_time;
	    // m_pParent->StreamTime(cur_time);
	    // rtStart = max(rtStart, cur_time);
		// hopefully this avoids this message/error:
		// [libmp3lame @ 00670aa0] Que input is backward in time
        // Audio timestamp 329016 < 329026 invalid, cliping00:05:29.05 bitrate= 738.6kbits/s
        // [libmp3lame @ 00670aa0] Que input is backward in time
	}

	// I once tried to change it to always have monotonicity of timestamps at this point, but it didn't fix any problems, and seems to do all right without it so maybe ok [?]
    m_rtPreviousSampleEndTime = rtStart + sampleTimeUsed;

	// NB that this *can* set it to a negative start time...hmm...which apparently is "ok" when a graph is just starting up it's expected...
    hr = pms->SetTime((REFERENCE_TIME*) &rtStart, (REFERENCE_TIME*) &m_rtPreviousSampleEndTime);
	if (FAILED(hr)) {
		assert(false);
        return hr;
    }
	// if we do SetTime(NULL, NULL) here then VLC can "play" it with directshows buffers of size 0ms.
	// however, then VLC cannot then stream it at all.  So we leave it set to some time, and just require you to have VLC buffers of at least 40 or 50 ms
	// [a possible VLC bug?] http://forum.videolan.org/viewtopic.php?f=14&t=92659&hilit=+50ms

	// whatever SetMediaTime even does...
    // hr = pms->SetMediaTime((REFERENCE_TIME*)&rtStart, (REFERENCE_TIME*)&m_rtPreviousSampleEndTime);
    //m_llSampleMediaTimeStart = m_rtSampleEndTime;

	if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    // Set the sample's properties.
    hr = pms->SetPreroll(FALSE); // tell it that this isn't preroll, so to actually use it...I think.
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }

    hr = pms->SetMediaType(NULL);
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }
   
    hr = pms->SetDiscontinuity(bFirstPacket); // true for the first
    if (FAILED(hr)) {
		assert(false);
        return hr;
    }
    
	// Set TRUE on every sample for PCM audio http://msdn.microsoft.com/en-us/library/windows/desktop/dd407021%28v=vs.85%29.aspx
    pms->SetSyncPoint(TRUE);
	if (FAILED(hr)) {
		assert(false);
        return hr;
    }

	bFirstPacket = false;

    return NOERROR;

} // end FillBuffer
