#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////


#include <streams.h>
#include <initguid.h>
#include <dllsetup.h>
#include <stdio.h>
#include "acam.h"

#pragma once

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

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
    m_paStreams[0] = new CVCamStream(phr, this, L"Virtual cam5");
}

STDMETHODIMP CVCam::Run(REFERENCE_TIME tStart) {
	((CVCamStream*) m_paStreams[0])->m_rtPreviousSampleEndTime = 0;
	return CSource::Run(tStart);
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

// I don't think this actually gets called when it's paused, but added here just in case... http://msdn.microsoft.com/en-us/library/dd377472%28v=vs.85%29.aspx
STDMETHODIMP CVCam::GetState(DWORD dw, FILTER_STATE *pState)
{
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
/* unimplemented yet...
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
    // Standard OLE stuff
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
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

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

HRESULT setupPwfex(WAVEFORMATEX *pwfex, AM_MEDIA_TYPE *pmt) {
	    // TODO match more than just htz...
	    // a "normal" audio stream...
		pwfex->wFormatTag = WAVE_FORMAT_PCM;
		pwfex->cbSize = 0;                  // apparently should be zero if using WAVE_FORMAT_PCM http://msdn.microsoft.com/en-us/library/ff538799(VS.85).aspx
		pwfex->nChannels = 2;               // 1 for mono, 2 for stereo..
		pwfex->nSamplesPerSec = getHtzRate();
		pwfex->wBitsPerSample = 16;          // 16 bit sound
		pwfex->nBlockAlign = (WORD)((pwfex->wBitsPerSample * pwfex->nChannels) / BITS_PER_BYTE);
        pwfex->nAvgBytesPerSec = pwfex->nSamplesPerSec * pwfex->nBlockAlign; // it can't calculate this itself? huh?
		
		// copy this info into the pmt
        return CreateAudioMediaType(pwfex, pmt, FALSE /* dont allocate more memory */); // not ours...
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

void setExpectedMaxBufferSize(long toThis);

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

    int nBitsPerSample = pwfexCurrent->wBitsPerSample;
    int nSamplesPerSec = pwfexCurrent->nSamplesPerSec;
    int nChannels = pwfexCurrent->nChannels;
	
    pProperties->cbBuffer = WaveBufferChunkSize;

    pProperties->cBuffers = (nChannels * nSamplesPerSec * nBitsPerSample) / 
                            (pProperties->cbBuffer * BITS_PER_BYTE);

	// Get 1/?? second worth of buffers, rounded to WaveBufferSize

    pProperties->cBuffers /= SECOND_FRACTIONS_TO_GRAB;

	// double check for underflow...
    if(pProperties->cBuffers < 1)
        pProperties->cBuffers = 1;
    

	// try with 1024B of buffer, just for fun.
	//pProperties->cbBuffer = 1024;
	//pProperties->cBuffers = 1;
	long maxBufferSize = pProperties->cbBuffer * pProperties->cBuffers;
	setExpectedMaxBufferSize(maxBufferSize);

    // Ask the allocator to reserve us the memory...

    ALLOCATOR_PROPERTIES Actual;
    HRESULT hr = pAlloc->SetProperties(pProperties,&Actual);
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
	ShowOutput("stop");
	return S_OK;
}


HRESULT CVCamStream::Exit()
{
	// never get here
    ShowOutput("exit method called");
	return S_OK;
}

int currentlyRunning = 0;

HRESULT CVCamStream::Inactive()
{
	// we get here
	ShowOutput("inactive: about to release loopback");
	loopBackRelease();
	ShowOutput("loopback released");
	currentlyRunning = 0; //allow it to restart later...
	return CSourceStream::Inactive(); //parent method
}


// Called when graph is first started
HRESULT CVCamStream::OnThreadCreate()
{
	assert(currentlyRunning == 0); // sanity...
	currentlyRunning = TRUE;
    GetMediaType(0, &m_mt); // give it a default type...

    HRESULT hr = LoopbackCaptureSetup();
    if (FAILED(hr)) {
       printf("IAudioCaptureClient::setup failed");
       return hr;
    }

    return NOERROR;
}

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
	// this is them saying you "must" use this type from now on...unless pmt is NULL that "means" reset...LODO handle it someday...
	assert(pmt);
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
	pASCC->MaximumSampleFrequency = getHtzRate();
	pASCC->MinimumSampleFrequency = getHtzRate();
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

