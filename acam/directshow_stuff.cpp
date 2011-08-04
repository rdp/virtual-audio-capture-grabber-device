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

REFERENCE_TIME latestGraphStart = 0;

STDMETHODIMP CVCam::Run(REFERENCE_TIME tStart) {
	latestGraphStart = tStart; // get one of these with each 'play' button, but not with pause [?]
	((CVCamStream*) m_paStreams[0])->m_rtSampleEndTime = latestGraphStart;
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

//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Virtual cam5"),phr, pParent, pPinName), m_pParent(pParent)
{
    // Set the media type...
    m_llSampleMediaTimeStart = 0;
	GetMediaType(0, &m_mt);

}

void loopBackRelease();

CVCamStream::~CVCamStream()
{
	//loopBackRelease(); no longer...
	int a = 3;
	ShowOutput("destructor");
} 

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
	return E_UNEXPECTED; // shouldn't call this...
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamOffset(REFERENCE_TIME *toHere) {
	if(latestGraphStart > 0) {
	  *toHere = latestGraphStart;
	  return S_OK;
	} else {
	  return E_UNEXPECTED;
	}
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetMaxStreamOffset(REFERENCE_TIME *toHere) {
  *toHere = 0; // ??
  return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::SetMaxStreamOffset(REFERENCE_TIME) {
	return E_UNEXPECTED; // ??
}

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

HRESULT setupPwfex(WAVEFORMATEX *pwfex, CMediaType *pmt) {
	    // NB this needs to "match" getScreenCaps
	    // a "normal" audio stream...
		pwfex->wFormatTag = WAVE_FORMAT_PCM;
		pwfex->cbSize=0; // apparently should be zero if using WAVE_FORMAT_PCM http://msdn.microsoft.com/en-us/library/ff538799(VS.85).aspx
		pwfex->nChannels = 2;
		pwfex->nSamplesPerSec = 44100;
		pwfex->wBitsPerSample = 16;
		pwfex->nAvgBytesPerSec = pwfex->nSamplesPerSec * pwfex->nChannels * pwfex->wBitsPerSample / BITS_PER_BYTE; // it can't calculate this itself? huh?
		pwfex->nBlockAlign = (WORD)((pwfex->wBitsPerSample * pwfex->nChannels) / BITS_PER_BYTE); // it can't calculate this itself?

		// copy this info into the pmt
        return CreateAudioMediaType(pwfex, pmt, FALSE); // not ours...
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

    // If iPosition equals zero then we always return the currently configured MediaType 
	// except we only have one, so no use in having it as a default :)
    /*if (iPosition == 0) {
        *pmt = m_mt;
        return S_OK;
    }*/

    WAVEFORMATEX *pwfex = (WAVEFORMATEX *) pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
	
	pmt->SetType(&MEDIATYPE_Audio);
	pmt->SetSubtype(&MEDIASUBTYPE_PCM);
    pmt->SetFormatType(&FORMAT_WaveFormatEx);
    pmt->SetTemporalCompression(FALSE);
	setupPwfex(pwfex, pmt);	
	pmt->SetSampleSize(pwfex->nAvgBytesPerSec/2); // .5s worth...

	return S_OK;
}

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{
	int cbFormat = pMediaType->cbFormat;
	int a = memcmp(pMediaType->pbFormat, m_mt.pbFormat, cbFormat);
	//BYTE pb1[18] = (BYTE [18]) pMediaType->pbFormat;
	//BYTE pb2[18] = (BYTE [18]) m_mt->pbFormat;
    if(*pMediaType != m_mt) {
        return E_INVALIDARG;
	}

    return S_OK;
} // CheckMediaType



// Size of each allocated buffer
// seems arbitrary
// maybe downstream needed a certain size?
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

	setExpectedMaxBufferSize(pProperties->cbBuffer * pProperties->cBuffers);

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

/* 
less useful, for VLC anyway..
HRESULT CVCamStream::Stop()
{
	return S_OK;
}


HRESULT CVCamStream::Exit()
{
	return S_OK;
}*/

HRESULT CVCamStream::Inactive() // sweet, also OnThreadDestroy seems to be called...but never for pause [?]
{
	ShowOutput("releasing loopback");
	loopBackRelease();
	ShowOutput("loopback released");
	return CSourceStream::Inactive();
}


// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_llSampleMediaTimeStart = 0;
    GetMediaType(0, &m_mt);

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
	// this is them saying you "must" use this type now...

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

	pAudioFormat->cbSize				= 0;
	pAudioFormat->wFormatTag			= WAVE_FORMAT_PCM;		// This is the wave format (needed for more than 2 channels)
	pAudioFormat->nSamplesPerSec		= 44100;	// This is in hertz
	pAudioFormat->nChannels				= 2;		// 1 for mono, 2 for stereo, and 4 because the camera puts out dual stereo
	pAudioFormat->wBitsPerSample		= 16;	// 16-bit sound
	pAudioFormat->nBlockAlign			= 2*16/8;
	pAudioFormat->nAvgBytesPerSec		= 44100*pAudioFormat->nBlockAlign; // TODO match our current audio settings [?]
	
	AM_MEDIA_TYPE * pm = *ppMediaType;

    pm->majortype = MEDIATYPE_Audio;
    pm->subtype = MEDIASUBTYPE_PCM;
    pm->formattype = FORMAT_WaveFormatEx;
    pm->bTemporalCompression = FALSE;
    pm->bFixedSizeSamples = TRUE;
    pm->lSampleSize = pAudioFormat->nBlockAlign; // appears reasonable http://github.com/tap/JamomaDSP/blob/2c80c487c6e560d959dd85e7d2bcca3a19ce9b26/src/os/win/DX/BaseClasses/mtype.cpp
    pm->cbFormat = sizeof(WAVEFORMATEX);
	pm->pUnk = NULL;

	AUDIO_STREAM_CONFIG_CAPS* pASCC = (AUDIO_STREAM_CONFIG_CAPS*) pSCC;
	ZeroMemory(pSCC, sizeof(AUDIO_STREAM_CONFIG_CAPS)); 

	// Set the audio capabilities
	pASCC->guid = MEDIATYPE_Audio;
	pASCC->ChannelsGranularity = 1;
	pASCC->MaximumChannels = 2;
	pASCC->MinimumChannels = 2;
	pASCC->MaximumSampleFrequency = 44100;
	pASCC->BitsPerSampleGranularity = 16;
	pASCC->MaximumBitsPerSample = 16;
	pASCC->MinimumBitsPerSample = 16;
	pASCC->MinimumSampleFrequency = 44100;
	pASCC->SampleFrequencyGranularity = 11025;

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

