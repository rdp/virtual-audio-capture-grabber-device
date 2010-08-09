// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <olectl.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////


#include <streams.h>
#include <initguid.h>
#include <dllsetup.h>
#include <malloc.h> // _alloca

#pragma once

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

EXTERN_C const GUID CLSID_VirtualCam;

class CVCamStream;
class CVCam : public CSource
{
public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFilterGraph *GetGraph() {return m_pGraph;}

private:
    CVCam(LPUNKNOWN lpunk, HRESULT *phr);
};

class CVCamStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet
{
public:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }                                                          \
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);
    
    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream
    //////////////////////////////////////////////////////////////////////////
    CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName);
    ~CVCamStream();

    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);
    
private:
    CVCam *m_pParent;
    REFERENCE_TIME m_rtLastTime;
    HBITMAP m_hLogoBmp;
    CCritSec m_cSharedState;
    IReferenceClock *m_pClock;
    bool m_fFirstSampleDelivered;

    CRefTime     m_rtSampleTime;    // The time to be stamped on each sample
    LONGLONG m_llSampleMediaTimeStart;

	HRESULT setAsNormal(CMediaType *pmt);

};




// TODO: reference additional headers your program requires here



//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME("Virtual Cam3"), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream **) new CVCamStream*[1];
    m_paStreams[0] = new CVCamStream(phr, this, L"Virtual Cam3");
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
    CSourceStream(NAME("Virtual Cam3"),phr, pParent, pPinName), m_pParent(pParent)
{

    // Set the media type...
    GetMediaType(0, &m_mt);
    m_fFirstSampleDelivered = FALSE;
    m_llSampleMediaTimeStart = 0;

}

CVCamStream::~CVCamStream()
{
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



HRESULT LoopbackCapture(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly);
const DWORD BITS_PER_BYTE = 8;

//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////

//
// FillBuffer
//
// Stuffs the buffer with data
// "they" call this
// LODO push versus pull?
// then "they" call Deliver...so I guess we just fill it with something?
// they *must* call this only every so often...
// you probably should fill the entire buffer...hmm...
HRESULT CVCamStream::FillBuffer(IMediaSample *pms) 
{
    CheckPointer(pms,E_POINTER);

    BYTE *pData;

    HRESULT hr = pms->GetPointer(&pData);
    if (FAILED(hr)) {
        return hr;
    }

    // This function must hold the state lock because it calls
    // FillPCMAudioBuffer().
    //CAutoLock lStateLock(m_pParent->pStateLock());
    
    // This lock must be held because this function uses
    // m_dwTempPCMBufferSize, m_hPCMToMSADPCMConversionStream,
    // m_rtSampleTime, m_fFirstSampleDelivered and
    // m_llSampleMediaTimeStart.
    // CAutoLock lShared(&m_cSharedState);

    WAVEFORMATEX* pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

    if (WAVE_FORMAT_PCM == pwfexCurrent->wFormatTag)
    {
        // old way
		// m_Synth->FillPCMAudioBuffer();

		// new way
		LoopbackCapture(pData, pms->GetSize(), NULL);

        hr = pms->SetActualDataLength(pms->GetSize());
        if (FAILED(hr))
            return hr;

    }
    else 
    {
		// who cares about ADPCM...
		return E_FAIL;
    }

    // Set the sample's start and end time stamps...
    CRefTime rtStart = m_rtSampleTime;

    m_rtSampleTime = rtStart + (REFERENCE_TIME)(UNITS * pms->GetActualDataLength()) / 
                     (REFERENCE_TIME)pwfexCurrent->nAvgBytesPerSec;

    hr = pms->SetTime((REFERENCE_TIME*)&rtStart, (REFERENCE_TIME*)&m_rtSampleTime);

    if (FAILED(hr)) {
        return hr;
    }

    // Set the sample's properties.
    hr = pms->SetPreroll(FALSE);
    if (FAILED(hr)) {
        return hr;
    }

    hr = pms->SetMediaType(NULL);
    if (FAILED(hr)) {
        return hr;
    }
   
    //hr = pms->SetDiscontinuity(!m_fFirstSampleDelivered);	
	hr = pms->SetDiscontinuity(TRUE);	
    if (FAILED(hr)) {
        return hr;
    }
    
    hr = pms->SetSyncPoint(!m_fFirstSampleDelivered);
	if (FAILED(hr)) {
        return hr;
    }

    LONGLONG llMediaTimeStart = m_llSampleMediaTimeStart;
    
    DWORD dwNumAudioSamplesInPacket = (pms->GetActualDataLength() * BITS_PER_BYTE) /
                                      (pwfexCurrent->nChannels * pwfexCurrent->wBitsPerSample);

    LONGLONG llMediaTimeStop = m_llSampleMediaTimeStart + dwNumAudioSamplesInPacket;

    hr = pms->SetMediaTime(&llMediaTimeStart, &llMediaTimeStop);
    if (FAILED(hr)) {
        return hr;
    }

    m_llSampleMediaTimeStart = llMediaTimeStop;
    m_fFirstSampleDelivered = TRUE;

    return NOERROR;
} // FillBuffer


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
    // I think we can ignore these...DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt); // TODO what does this do, and why do I set it as null later?
    return hr;
}

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);


HRESULT CVCamStream::setAsNormal(CMediaType *pmt) {
	    WAVEFORMATEX *pwfex;
        pwfex = (WAVEFORMATEX *) pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
        if(NULL == pwfex)
        {
            return E_OUTOFMEMORY;
        }

		// we'll set it as PCM or what not, within this call...

		// now tell them "this is what we will give you..."
		LoopbackCapture(NULL, -1, pwfex);
        
		// old sin wave way...
		//m_Synth->GetPCMFormatStructure(pwfex);

    return CreateAudioMediaType(pwfex, pmt, FALSE); // not ours...

}



// GetMediaType
// I believe "they" call this...
// we only have one type at a time...
// so we just return our one type...
// which we already told them what it was.
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType *pmt) 
{
	// no more positioning...
	if(iPosition != 0)
		return E_FAIL; // huh?

    CheckPointer(pmt,E_POINTER);

    // The caller must hold the state lock because this function
    // calls get_OutputFormat() and GetPCMFormatStructure().
    // The function assumes that the state of the m_Synth
    // object does not change between the two calls.  The
    // m_Synth object's state will not change if the 
    // state lock is held.
    // fails currently lodo ...ASSERT(CritCheckIn(m_pParent->pStateLock()));

	return setAsNormal(pmt);	
}

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{
    // huh? VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
    if(*pMediaType != m_mt) 
        return E_INVALIDARG;

    return S_OK;
} // CheckMediaType


const int WaveBufferSize = 16*1024;     // Size of each allocated buffer
// seems arbitrary
// maybe downstream needed it?

// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated. So we have a look at m_mt to see what format we agreed to.
// Then we can ask for buffers of the correct size to contain them.
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc,
                                       ALLOCATOR_PROPERTIES *pProperties)
{
    // The caller should always hold the shared state lock 
    // before calling this function.  This function must hold 
    // the shared state lock because it uses m_hPCMToMSADPCMConversionStream
    // m_dwTempPCMBufferSize.
    // fails currently. odd...lodo ... ASSERT(CritCheckIn(&m_cSharedState));

    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);

    WAVEFORMATEX *pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

    if(WAVE_FORMAT_PCM == pwfexCurrent->wFormatTag)
    {
        pProperties->cbBuffer = WaveBufferSize; // guess 16K is standard for PCM? what?
    }
    else
    {
		// no ADMCP!
        return E_FAIL;        
    }

    int nBitsPerSample = pwfexCurrent->wBitsPerSample;
    int nSamplesPerSec = pwfexCurrent->nSamplesPerSec;
    int nChannels = pwfexCurrent->nChannels;

	// Get 1 second worth of buffers

    pProperties->cBuffers = (nChannels * nSamplesPerSec * nBitsPerSample) / 
                            (pProperties->cbBuffer * BITS_PER_BYTE);

    // Get 1/2 second worth of buffers
    pProperties->cBuffers /= 2;
    if(pProperties->cBuffers < 1)
        pProperties->cBuffers = 1 ;

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

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    return NOERROR;
} // OnThreadCreate


//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
	// you "must" use this type forever now...
	// do we need anything audio-y here?
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
    *piCount = 1; // only allow one type...
    *piSize = sizeof(AUDIO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
	if(iIndex != 0)
		return E_FAIL; // huh?

    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(AUDIO_STREAM_CONFIG_CAPS, pvi, (*pmt)->pbFormat);
	
    WAVEFORMATEX* pwfex = (WAVEFORMATEX *) _alloca(sizeof(WAVEFORMATEX));

	LoopbackCapture(NULL, -1, pwfex);

	pvi->BitsPerSampleGranularity = 8; // huh?
	pvi->ChannelsGranularity = 1;
	pvi->guid = MEDIATYPE_Audio; // theoretically they set this for us
	pvi->MaximumBitsPerSample = pwfex->wBitsPerSample;
	pvi->MaximumChannels = pwfex->nChannels;
	pvi->MaximumSampleFrequency = pwfex->nSamplesPerSec;
	pvi->MinimumBitsPerSample = pwfex->wBitsPerSample;
	pvi->MinimumChannels= pwfex->nChannels;
	pvi->MinimumSampleFrequency = pwfex->nSamplesPerSec;
	pvi->SampleFrequencyGranularity = 11025; // from http://msdn.microsoft.com/en-us/library/dd317597(VS.85).aspx

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


#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")

#ifdef _DEBUG
    #pragma comment(lib, "strmbasd")
#else
    #pragma comment(lib, "strmbase")
#endif

#include <olectl.h>
#include <initguid.h>
#include <dllsetup.h>

#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer( CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType     = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer( CLSID clsServer );



// {8E14549A-DB61-4309-AFA1-3578E927E933}
DEFINE_GUID(CLSID_VirtualCam,
            0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33);


const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam = 
{ &MEDIATYPE_Audio      // clsMajorType
, &MEDIASUBTYPE_NULL }; // clsMinorType

const AMOVIESETUP_PIN AMSPinVCam=
{
    L"Output",             // Pin string name
    FALSE,                 // Is it rendered
    TRUE,                  // Is it an output
    FALSE,                 // Can we have none
    FALSE,                 // Can we have many
    &CLSID_NULL,           // Connects to filter
    NULL,                  // Connects to pin
    1,                     // Number of types
    &AMSMediaTypesVCam      // Pin Media types
};

const AMOVIESETUP_FILTER AMSFilterVCam =
{
    &CLSID_VirtualCam,  // Filter CLSID
    L"Virtual Cam3",     // String name
    MERIT_DO_NOT_USE,      // Filter merit
    1,                     // Number pins
    &AMSPinVCam             // Pin details
};

CFactoryTemplate g_Templates[] = 
{
    {
        L"Virtual Cam3",
        &CLSID_VirtualCam,
        CVCam::CreateInstance,
        NULL,
        &AMSFilterVCam
    },

};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// straight call to here on init, instead of to
// AMovieDllRegisterServer2 which is what the other fella does...
// which I assume is similar to this...maybe?
STDAPI RegisterFilters( BOOL bRegister )
{
    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
    char achTemp[MAX_PATH];
    ASSERT(g_hInst != 0);

    if( 0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp))) 
        return AmHresultFromWin32(GetLastError());

    MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1, 
                       achFileName, NUMELMS(achFileName));
  
    hr = CoInitialize(0);
    if(bRegister)
    {
        hr = AMovieSetupRegisterServer(CLSID_VirtualCam, L"Virtual Cam3", achFileName, L"Both", L"InprocServer32");
    }

    if( SUCCEEDED(hr) )
    {
        IFilterMapper2 *fm = 0;
        hr = CreateComObject( CLSID_FilterMapper2, IID_IFilterMapper2, fm );
        if( SUCCEEDED(hr) )
        {
            if(bRegister)
            {
                IMoniker *pMoniker = 0;
                REGFILTER2 rf2;
                rf2.dwVersion = 1;
                rf2.dwMerit = MERIT_DO_NOT_USE;
                rf2.cPins = 1;
                rf2.rgPins = &AMSPinVCam;
                hr = fm->RegisterFilter(CLSID_VirtualCam, L"Virtual Cam3", &pMoniker, &CLSID_AudioInputDeviceCategory, NULL, &rf2);
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_AudioInputDeviceCategory, 0, CLSID_VirtualCam);
            }
        }

      // release interface
      //
      if(fm)
          fm->Release();
    }

    if( SUCCEEDED(hr) && !bRegister )
        hr = AMovieSetupUnregisterServer( CLSID_VirtualCam );

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// DLL.cpp

//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>

STDAPI RegisterFilters( BOOL bRegister );

STDAPI DllRegisterServer()
{
	printf("hello there"); // we never see this...
    return RegisterFilters(TRUE);
}

STDAPI DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

STDAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

extern "C" BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

