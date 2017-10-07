#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////


#include <streams.h>
#include <dllsetup.h>
#include <stdio.h>
#include "acam.h"

#pragma once

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

extern long expectedMaxBufferSize;
extern long pBufOriginalSize; 

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);

	return punk;
}

IReferenceClock *globalClock;

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME("Virtual cam5"), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
	//m_pClock is null at this point...
    m_paStreams = (CSourceStream **) new CVCamStream*[1];
    m_paStreams[0] = new CVCamStream(phr, this, L"Capture Virtual Audio Pin");
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

// this does seem to get called frequently... http://msdn.microsoft.com/en-us/library/dd377472%28v=vs.85%29.aspx
// I believe theoretically return VFW_S_CANT_CUE means "don't call me for data" so we should be safe here [?] but somehow still FillBuffer is called :|
STDMETHODIMP CVCam::GetState(DWORD dw, FILTER_STATE *pState)
{
	//ShowOutput("GetState call ed");
    CheckPointer(pState, E_POINTER);
    *pState = m_State;
    if (m_State == State_Paused)
    {
        return VFW_S_CANT_CUE;
    }
    else
    {
        return S_OK;
    }
}


//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Virtual cam5"),phr, pParent, pPinName), m_pParent(pParent)
{
    // Set the media type...
	GetMediaType(0, &m_mt);
}

void loopBackRelease();

CVCamStream::~CVCamStream()
{
	// don't get here with any consistency...
	ShowOutput("destructor");
} 

// these latency/pushsource stuffs never seem to get called...ever...at least by VLC...
/* therefore unimplemented yet...
HRESULT STDMETHODCALLTYPE CVCamStream::GetLatency(REFERENCE_TIME *storeItHere) {
	*storeItHere = 10000/SECOND_FRACTIONS_TO_GRAB;
	// 1_000_000ns per s, this is in 100 ns or 10_000/s
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetPushSourceFlags(ULONG *storeHere) {
	storeHere = 0; // the default (0) is ok...
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::SetPushSourceFlags(ULONG storeHere) {
	return E_UNEXPECTED; // shouldn't ever call this...
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamOffset(REFERENCE_TIME *toHere) {
	return E_UNEXPECTED; // shouldn't ever call this...

//  *toHere = m_tStart; // guess this is right... huh? offset? to what?
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetMaxStreamOffset(REFERENCE_TIME *toHere) {
  *toHere = 0; // TODO set this to a reasonable value...
  return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::SetMaxStreamOffset(REFERENCE_TIME) {
	return E_UNEXPECTED; // I don't think they'd ever call this either...
}*/

HRESULT CVCamStream::QueryInterface(REFIID riid, void **ppv)
{   
    // Standard OLE stuff...allow it to query for our interfaces that we implement...
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
	else if(riid == _uuidof(IAMBufferNegotiation))
		 *ppv = (IAMBufferNegotiation*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}

//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType *pmt)
{
	// call the base class' SetMediaType...
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

HRESULT setupPwfex(WAVEFORMATEX *pwfex, AM_MEDIA_TYPE *pmt) { // called super early during negotiation...
	// we prefer PCM since WAVE_FORMAT_EXTENSIBLE seems to cause IFilterGraph::DirectConnect to fail, and many programs use it [VLC maybe does?]
	// also we set it up to auto convert to PCM for us
	// see also the bInt16 setting
	pwfex->wFormatTag = WAVE_FORMAT_PCM;
	pwfex->cbSize = 0;                  // apparently should be zero if using WAVE_FORMAT_PCM http://msdn.microsoft.com/en-us/library/ff538799(VS.85).aspx
	pwfex->nChannels = getChannels();               // 1 for mono, 2 for stereo..
	HRESULT hr;
	pwfex->nSamplesPerSec = getHtzRate(&hr);
	if (hr != S_OK) {
		return hr;
	}
	pwfex->wBitsPerSample = 16;          // 16 bit sound
	pwfex->nBlockAlign = (WORD)((pwfex->wBitsPerSample * pwfex->nChannels) / BITS_PER_BYTE);
	pwfex->nAvgBytesPerSec = pwfex->nSamplesPerSec * pwfex->nBlockAlign; // it can't calculate this itself? huh?
		
	// copy this info into the pmt
    hr = ::CreateAudioMediaType(pwfex, pmt, FALSE /* dont allocate more memory */);
	return hr;
}

HRESULT CVCamStream::setAsNormal(CMediaType *pmt) {
  WAVEFORMATEX *pwfex;
  pwfex = (WAVEFORMATEX *) pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
  ZeroMemory(pwfex, sizeof(WAVEFORMATEX));
  if(NULL == pwfex)
   {
       return E_OUTOFMEMORY;
   }
   return setupPwfex(pwfex, pmt);
}

// GetMediaType
// I believe "they" call this...
// we only have one type at a time...
// so we just return our one type...
// which we already told them what it was.
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType *pmt) 
{
    if (iPosition < 0) {
        return E_INVALIDARG;
    }

	if (iPosition > 0) {
        return VFW_S_NO_MORE_ITEMS;
	}

    WAVEFORMATEX *pwfex = (WAVEFORMATEX *) pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
	
	setupPwfex(pwfex, pmt);
	return S_OK;
}

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{
	int cbFormat = pMediaType->cbFormat;
    if(*pMediaType != m_mt) {
        return E_INVALIDARG;
	}
    return S_OK;
} // CheckMediaType


// Size of each allocated buffer
// seems arbitrary
// maybe downstream needs a certain size?
const int WaveBufferChunkSize = 16*1024;     

// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated. So we have a look at m_mt to see what format we agreed to.
// Then we can ask for buffers of the correct size to contain them.
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc,
                                       ALLOCATOR_PROPERTIES *pProperties)
{
    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);

    WAVEFORMATEX *pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

	// just use our max size for the buffer size (or whatever they specified for us, if they did)
	pProperties->cBuffers = 1;
	pProperties->cbBuffer = expectedMaxBufferSize;

    // Ask the allocator to reserve us this much memory...
	
    ALLOCATOR_PROPERTIES Actual;
    HRESULT hr = pAlloc->SetProperties(pProperties, &Actual);
    if(FAILED(hr))
    {
        return hr;
    }

    // Is this allocator unsuitable

    if(Actual.cbBuffer < pProperties->cbBuffer)
    {
        return E_FAIL;
    }

    return NOERROR;

} // DecideBufferSize

HRESULT LoopbackCaptureSetup();

HRESULT CVCamStream::OnThreadDestroy()
{
	ShowOutput("OnThreadDestroy");
	return S_OK; 
}

//less useful, for VLC anyway..
HRESULT CVCamStream::Stop()
{
	// never get here
	ShowOutput("pin stop called");
	// call Inactive to initiate shutdown anyway just in case :)
	Inactive();
	return S_OK;
}

HRESULT CVCamStream::Exit()
{
	// never seem to get here
    ShowOutput("exit called");
	return S_OK;
}

int currentlyRunning = 0;

HRESULT CVCamStream::Inactive()
{
	// we get here at least with vlc
	ShowOutput("stream inactive called");
	return CSourceStream::Inactive(); //parent method
}

STDMETHODIMP CVCam::Stop()
{
	ShowOutput("parent stop called");
	if (currentlyRunning)
	{
      ShowOutput("about to release loopback");
	  loopBackRelease();
	  ShowOutput("loopback released");
  	  currentlyRunning = 0; // allow it to restart later...untested...
	}

	return CSource::Stop();
}

STDMETHODIMP CVCam::Pause()
{
	// pause also gets called at "graph setup" time somehow before an initial Run, and always, weirdly...
	// but not at "teardown" or "stop" time apparently...
	// but yes at "pause" time as it were (though some requests for packets still occur, even after that, yikes!)
	// pause is also called immediately before Stop...yikes!
	// so the order is pause, run, pause, stop
	// or, with a pause in there
	// pause, run, pause, run, pause, stop
	// or have I seen run pause before, initially [?] ai ai
	// unfortunately after each pause it still "requires" a few samples out or the graph will die (well, as we're doing it ???)
	// see also http://microsoft.public.multimedia.directx.dshow.programming.narkive.com/h8ZxbM9E/csourcestream-fillbuffer-timing this thing shucks..huh?
	// so current plan: just start it once, leave it going...but when Run is called it clears a buffer somewhere...
	ShowOutput("parent pause called");
	return CBaseFilter::Pause();
}

// Called when graph is first started
HRESULT CVCamStream::OnThreadCreate()
{
	ShowOutput("OnThreadCreate called"); // seems like this gets called pretty early...
    GetMediaType(0, &m_mt); // give it a default type...do we even  need this? LOL

	assert(currentlyRunning == 0); // sanity check no double starts...
	currentlyRunning = TRUE;

    HRESULT hr = LoopbackCaptureSetup();
    if (FAILED(hr)) {
       printf("IAudioCaptureClient::setup failed");
       return hr;
    }

    return NOERROR;
}
void LoopbackCaptureClear();
STDMETHODIMP CVCam::Run(REFERENCE_TIME tStart) {
	ShowOutput("Parent Run called");
	LoopbackCaptureClear(); // post pause, want to clear this to alert it to use the right time stamps...I think hope!

	// LODO why is this called seemingly *way* later than packets are already collected? I guess it calls Pause first or something, to let filters "warm up" ... so in essence, this means "unPause! go!"
	// ((CVCamStream*) m_paStreams[0])->m_rtPreviousSampleEndTime = 0;
	// looks like we accomodate for "resetting" within our own discontinuity stuff...
	// LODO should we...umm...not give any samples before second one or not here?
	return CSource::Run(tStart); // this just calls CBaseFilter::Run (which calls Run on all the pins *then* sets its state to running)
}

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
	// this is them saying you "must" use this type from now on...unless pmt is NULL that "means" reset...LODO handle it someday...
	if(!pmt) {
	  return S_OK; // *sure* we reset..yeah...sure we did...
	}
	
   if(CheckMediaType((CMediaType *) pmt) != S_OK) {
	return E_FAIL; // just in case :P [FME...]
   }
	
	m_mt = *pmt;

	IPin* pin; 
	ConnectedTo(&pin);
	if(pin)
	{
		IFilterGraph *pGraph = m_pParent->GetGraph();
		pGraph->Reconnect(this);
	}

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 1; // only allow one type currently...
    *piSize = sizeof(AUDIO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **ppMediaType, BYTE *pSCC)
{	 
	
	if(iIndex < 0)
		return E_INVALIDARG;
	if(iIndex > 0)
		return S_FALSE;
	if(pSCC == NULL)
		return E_POINTER;

    *ppMediaType = CreateMediaType(&m_mt);
	if (*ppMediaType == NULL) return E_OUTOFMEMORY;

    DECLARE_PTR(WAVEFORMATEX, pAudioFormat, (*ppMediaType)->pbFormat);
	
	AM_MEDIA_TYPE * pm = *ppMediaType;
	
	setupPwfex(pAudioFormat, pm);

	AUDIO_STREAM_CONFIG_CAPS* pASCC = (AUDIO_STREAM_CONFIG_CAPS*) pSCC;
	ZeroMemory(pSCC, sizeof(AUDIO_STREAM_CONFIG_CAPS)); 

	// Set up audio capabilities [one type only, for now]
	pASCC->guid = MEDIATYPE_Audio;
	pASCC->MaximumChannels = pAudioFormat->nChannels;
	pASCC->MinimumChannels = pAudioFormat->nChannels;
	pASCC->ChannelsGranularity = 1; // doesn't matter
	pASCC->MaximumSampleFrequency = pAudioFormat->nSamplesPerSec;
	pASCC->MinimumSampleFrequency = pAudioFormat->nSamplesPerSec;
	pASCC->SampleFrequencyGranularity = 11025; // doesn't matter
	pASCC->MaximumBitsPerSample = pAudioFormat->wBitsPerSample;
	pASCC->MinimumBitsPerSample = pAudioFormat->wBitsPerSample;
	pASCC->BitsPerSampleGranularity = 16; // doesn't matter

	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////

HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
        
    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CVCamStream::SuggestAllocatorProperties( /* [in] */ const ALLOCATOR_PROPERTIES *pprop) {
	// maybe we shouldn't even care though...I mean like seriously...why let them make it smaller <sigh>
	// LODO test it both ways with FME, fast computer/slow computer does it make a difference?
	int requested = pprop->cbBuffer;
	if(pprop->cBuffers > 0)
	    requested *= pprop->cBuffers;
	if(pprop->cbPrefix > 0)
	    requested += pprop->cbPrefix;
	
	if(requested <= pBufOriginalSize) {
 		expectedMaxBufferSize = requested;
		return S_OK; // they requested it? ok you got it! You're requesting possible problems! oh well! you requested it!
	} else {
		return E_FAIL;
	}
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetAllocatorProperties( ALLOCATOR_PROPERTIES *pprop) {return NULL;} // they never call this...
