//------------------------------------------------------------------------------
// File: Synth.cpp
//
// Desc: DirectShow sample code - implements an audio signal generator
//       source filter.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------


// NB I don't know if directsource *can* actually work with VLC
// I doub it
//I.e. this particular sub-project fella prolly can't work as a VLC source
#include <windows.h>
#include <streams.h>
#include <malloc.h>

#include <math.h>
#include <mmreg.h>
#include <msacm.h>

#include <initguid.h>
#if (1100 > _MSC_VER)
#include <olectlid.h>
#else
#include <olectl.h>
#endif


#define _AUDIOSYNTH_IMPLEMENTATION_

#include "DynSrc.h"
#include "isynth.h"
#include "synth.h"
#include "synthprp.h"
#include <stdio.h>

// setup data

const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{ &MEDIATYPE_Audio      // clsMajorType
, &MEDIASUBTYPE_NULL }; // clsMinorType

const AMOVIESETUP_PIN sudOpPin =
{ L"Output"          // strName
, FALSE              // bRendered
, TRUE               // bOutput
, FALSE              // bZero
, FALSE              // bMany
, &CLSID_NULL        // clsConnectsToFilter
, L"Input"           // strConnectsToPin
, 1                  // nTypes
, &sudOpPinTypes };  // lpTypes

const AMOVIESETUP_FILTER sudSynth =
{ &CLSID_SynthFilter     // clsID
, L"Audio Synthesizer" // strName
, MERIT_UNLIKELY       // dwMerit
, 1                    // nPins
, &sudOpPin };         // lpPin

// -------------------------------------------------------------------------
// g_Templates
// -------------------------------------------------------------------------
// COM global table of objects in this dll

CFactoryTemplate g_Templates[] = {

    { L"Audio Synthesizer"
    , &CLSID_SynthFilter
    , CSynthFilter::CreateInstance
    , NULL
    , &sudSynth }
  ,
    { L"Audio Synthesizer Property Page"
    , &CLSID_SynthPropertyPage
    , CSynthProperties::CreateInstance }

};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);



// -------------------------------------------------------------------------
// CSynthFilter, the main filter object
// -------------------------------------------------------------------------
//
// CreateInstance
//
// The only allowed way to create Synthesizers

CUnknown * WINAPI CSynthFilter::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr) 
{
    ASSERT(phr);
    
    CUnknown *punk = new CSynthFilter(lpunk, phr);
    if (punk == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return punk;
}


//
// CSynthFilter::Constructor
//
// initialise a CSynthStream object so that we have a pin.

CSynthFilter::CSynthFilter(LPUNKNOWN lpunk, HRESULT *phr)
    : CDynamicSource(NAME("Audio Synthesizer Filter"),lpunk, CLSID_SynthFilter, phr)
    , CPersistStream(lpunk, phr)
{
    m_paStreams = (CDynamicSourceStream **) new CSynthStream*[1];
    if (m_paStreams == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

    m_paStreams[0] = new CSynthStream(phr, this, L"Audio Synth Stream");
    if (m_paStreams[0] == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

    if (SUCCEEDED(*phr)) {
        ASSERT(m_Synth);
        m_Synth->put_SynthFormat(1,     // Channels
                                 8,     // Bits Per Sample
                                 11025  // Samples Per Sececond
                                 );
    }
}



STDMETHODIMP CSynthFilter::GetClassID(CLSID *pClsid)
{
    return CBaseFilter::GetClassID(pClsid);
}


int CSynthFilter::SizeMax ()
{
    return sizeof (int) * 8;
}



DWORD CSynthFilter::GetSoftwareVersion(void)
{
    return 1;
}



// -------------------------------------------------------------------------
// CSynthStream, the output pin
// -------------------------------------------------------------------------

//
// CSynthStream::Constructor
//

CSynthStream::CSynthStream(HRESULT *phr, CSynthFilter *pParent, LPCWSTR pName)
    : CDynamicSourceStream(NAME("Audio Synth output pin"),phr, pParent, pName)
    , m_hPCMToMSADPCMConversionStream(NULL)
    , m_dwTempPCMBufferSize(0)
    , m_fFirstSampleDelivered(FALSE)
    , m_llSampleMediaTimeStart(0) 
{
    ASSERT(phr);

    m_Synth = new CAudioSynth(pParent->pStateLock());

    pParent->m_Synth = m_Synth;
    if (m_Synth == NULL) {
        *phr = E_OUTOFMEMORY;
        return;
    }

    m_pParent = pParent;
}


//
// CSynthStream::Destructor
//
CSynthStream::~CSynthStream(void) 
{
    delete m_Synth;
}


HRESULT LoopbackCapture(const WAVEFORMATEX& wfex, BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly);


//
// FillBuffer
//
// Stuffs the buffer with data
// "they" call this
// then "they" call Deliver...so I guess we just fill it with something?
// they *must* call this only every so often...
// you probably should fill the entire buffer...hmm...
HRESULT CSynthStream::FillBuffer(IMediaSample *pms) 
{
    CheckPointer(pms,E_POINTER);

    BYTE *pData;

    HRESULT hr = pms->GetPointer(&pData);
    if (FAILED(hr)) {
        return hr;
    }

    // This function must hold the state lock because it calls
    // FillPCMAudioBuffer().
    CAutoLock lStateLock(m_pParent->pStateLock());
    
    // This lock must be held because this function uses
    // m_dwTempPCMBufferSize, m_hPCMToMSADPCMConversionStream,
    // m_rtSampleTime, m_fFirstSampleDelivered and
    // m_llSampleMediaTimeStart.
    CAutoLock lShared(&m_cSharedState);

    WAVEFORMATEX* pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

    if (WAVE_FORMAT_PCM == pwfexCurrent->wFormatTag) 
    {
        // old way
		// m_Synth->FillPCMAudioBuffer();

		// new way
		LoopbackCapture(*pwfexCurrent, pData, pms->GetSize(), NULL);

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

	//...CDynamicSourceStream::SetMediaType(
    hr = pms->SetMediaType(NULL);
    if (FAILED(hr)) {
        return hr;
    }
   
    hr = pms->SetDiscontinuity(!m_fFirstSampleDelivered);
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
}


//
// Format Support
//

//
// GetMediaType
// I believe "they" call this...
// we only have one type at a time...
// so we just return our one type...
// which we already told them what it was.
HRESULT CSynthStream::GetMediaType(CMediaType *pmt) 
{
    CheckPointer(pmt,E_POINTER);

    // The caller must hold the state lock because this function
    // calls get_OutputFormat() and GetPCMFormatStructure().
    // The function assumes that the state of the m_Synth
    // object does not change between the two calls.  The
    // m_Synth object's state will not change if the 
    // state lock is held.
    ASSERT(CritCheckIn(m_pParent->pStateLock()));

	return setAsNormal(pmt);	
}

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);


HRESULT CSynthStream::setAsNormal(CMediaType *pmt) {
	    WAVEFORMATEX *pwfex;
    SYNTH_OUTPUT_FORMAT ofCurrent;

    HRESULT hr = m_Synth->get_OutputFormat( &ofCurrent ); // get PCM
    if(FAILED(hr))
    {
        return hr;
    }
    
    if(SYNTH_OF_PCM == ofCurrent)
    {
		// we always get here...
        pwfex = (WAVEFORMATEX *) pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
        if(NULL == pwfex)
        {
            return E_OUTOFMEMORY;
        }

		// we'll set it as PCM or what not, within this call...

		// now tell them "this is what we will give you..."
		WAVEFORMATEX unused;
		LoopbackCapture(unused, NULL, -1, pwfex);
        
		// old sin wave way...
		//m_Synth->GetPCMFormatStructure(pwfex);

    }
    else if(SYNTH_OF_MS_ADPCM == ofCurrent)
    {
		  // !no ADPCM!
		  return E_FAIL;
    }
    else
    {
        return E_UNEXPECTED;
    }

    return CreateAudioMediaType(pwfex, pmt, FALSE); // not ours...

}

// other fella doesn't even have this one...
// maybe it could if it wanted to...
HRESULT CSynthStream::CompleteConnect(IPin *pReceivePin)
{
    // This lock must be held because this function uses
    // m_hPCMToMSADPCMConversionStream, m_fFirstSampleDelivered 
    // and m_llSampleMediaTimeStart.
    CAutoLock lShared(&m_cSharedState);

    HRESULT hr;
    WAVEFORMATEX *pwfexCurrent = (WAVEFORMATEX*)m_mt.Format();

    if(WAVE_FORMAT_PCM == pwfexCurrent->wFormatTag)
    {
		// always create our pretty sin wave on connect...
        hr = m_Synth->AllocWaveCache(*pwfexCurrent);
        if(FAILED(hr))
        {
            return hr;
        }
    }
    else if(WAVE_FORMAT_ADPCM == pwfexCurrent->wFormatTag)
    {
		// no ADPCM!
		return E_FAIL;
    }
    else
    {
        ASSERT(NULL == m_hPCMToMSADPCMConversionStream);
    }

    hr = CDynamicSourceStream::CompleteConnect(pReceivePin);
    if(FAILED(hr))
    {
        if(WAVE_FORMAT_ADPCM == pwfexCurrent->wFormatTag)
        {
            // acmStreamClose() should never fail because m_hPCMToMSADPCMConversionStream
            // holds a valid ACM stream handle and all operations using the handle are 
            // synchronous.
            EXECUTE_ASSERT(0 == acmStreamClose(m_hPCMToMSADPCMConversionStream, 0));
            m_hPCMToMSADPCMConversionStream = NULL;
        }

        return hr;
    }

    m_fFirstSampleDelivered = FALSE;
    m_llSampleMediaTimeStart = 0;

    return S_OK;
}


// pin connect was broken
HRESULT CSynthStream::BreakConnect(void)
{
    // This lock must be held because this function uses
    // m_hPCMToMSADPCMConversionStream and m_dwTempPCMBufferSize.
    CAutoLock lShared(&m_cSharedState);

    HRESULT hr = CDynamicSourceStream::BreakConnect();
    if(FAILED(hr))
    {
        return hr;
    }

    if(NULL != m_hPCMToMSADPCMConversionStream)
    {
        // acmStreamClose() should never fail because m_hPCMToMSADPCMConversionStream
        // holds a valid ACM stream handle and all operations using the handle are 
        // synchronous.
        EXECUTE_ASSERT(0 == acmStreamClose(m_hPCMToMSADPCMConversionStream, 0));
        m_hPCMToMSADPCMConversionStream = NULL;
        m_dwTempPCMBufferSize = 0;
    }

    return S_OK;
}


const int WaveBufferSize = 16*1024;     // Size of each allocated buffer
// arbitrary? what the heck? maybe downstream needed something larger than what we were giving it or something?
// maybe doesn't matter, as long as it's "big enough"?


//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated. So we have a look at m_mt to see what format we agreed to.
// Then we can ask for buffers of the correct size to contain them.
HRESULT CSynthStream::DecideBufferSize(IMemAllocator *pAlloc,
                                       ALLOCATOR_PROPERTIES *pProperties)
{
    // The caller should always hold the shared state lock 
    // before calling this function.  This function must hold 
    // the shared state lock because it uses m_hPCMToMSADPCMConversionStream
    // m_dwTempPCMBufferSize.
    ASSERT(CritCheckIn(&m_cSharedState));

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
}


    // switch the pin to active (paused or running) mode
    // not an error to call this if already active
//
// Active
//
HRESULT CSynthStream::Active(void)
{
    // This lock must be held because the function
    // uses m_rtSampleTime, m_fFirstSampleDelivered
    // and m_llSampleMediaTimeStart.
    CAutoLock lShared(&m_cSharedState);

    HRESULT hr = CDynamicSourceStream::Active();
    if(FAILED(hr))
    {
        return hr;
    }

    m_rtSampleTime = 0;
    m_fFirstSampleDelivered = FALSE;
    m_llSampleMediaTimeStart = 0;

    return NOERROR;
}


// -------------------------------------------------------------------------
// CAudioSynth
// -------------------------------------------------------------------------
// Object that knows nothing about DirectShow, but just synthesizes waveforms

CAudioSynth::CAudioSynth(
                CCritSec* pStateLock,
                int Frequency,
                int Waveform,
                int iBitsPerSample,
                int iChannels,
                int iSamplesPerSec,
                int iAmplitude
                )
    : m_bWaveCache(NULL)
    , m_wWaveCache(NULL)
    , m_pStateLock(pStateLock)
{
    ASSERT(Waveform >= WAVE_SINE);
    ASSERT(Waveform <  WAVE_LAST);

    m_iFrequency = Frequency;
    m_iWaveform = Waveform;
    m_iAmplitude = iAmplitude;
    m_iSweepStart = DefaultSweepStart;
    m_iSweepEnd = DefaultSweepEnd;

    m_wFormatTag = WAVE_FORMAT_PCM;
    m_wBitsPerSample = (WORD) iBitsPerSample;
    m_wChannels = (WORD) iChannels;
    m_dwSamplesPerSec = iSamplesPerSec;
}


CAudioSynth::~CAudioSynth()
{
    if(m_bWaveCache)
    {
        delete[] m_bWaveCache;
    }

    if(m_wWaveCache)
    {
        delete[] m_wWaveCache;
    }
}


//
// AllocWaveCache
//
//
HRESULT CAudioSynth::AllocWaveCache(const WAVEFORMATEX& wfex)
{
    // The caller should hold the state lock because this
    // function uses m_iWaveCacheCycles, m_iWaveCacheSize
    // m_iFrequency, m_bWaveCache and m_wWaveCache.  The
    // function should also hold the state lock because
    // it calls CalcCache().
    ASSERT(CritCheckIn(m_pStateLock));

    m_iWaveCacheCycles = m_iFrequency;
    m_iWaveCacheSize = (int) wfex.nSamplesPerSec;

    if(m_bWaveCache)
    {
        delete[] m_bWaveCache;
        m_bWaveCache = NULL;
    }
    if(m_wWaveCache)
    {
        delete[] m_wWaveCache;
        m_wWaveCache = NULL;
    }

    // The wave cache always stores PCM audio data.
    if(wfex.wBitsPerSample == 8)
    {
        m_bWaveCache = new BYTE [m_iWaveCacheSize];
        if(NULL == m_bWaveCache)
        {
            return E_OUTOFMEMORY;
        }
    }
    else
    {
        m_wWaveCache = new WORD [m_iWaveCacheSize];
        if(NULL == m_wWaveCache)
        {
            return E_OUTOFMEMORY;
        }
    }

    CalcCache(wfex); // fill it with a sin wave...

    return S_OK;
}


// I don't think this is ever called anymore...

void CAudioSynth::GetPCMFormatStructure(WAVEFORMATEX* pwfex)
{
    ASSERT(pwfex);

    // The caller must hold the state lock because this function uses
    // m_wChannels, m_wBitsPerSample and m_dwSamplesPerSec.
    ASSERT(CritCheckIn(m_pStateLock));

    // Check for valid input parametes.
    ASSERT((1 == m_wChannels) || (2 == m_wChannels));
    ASSERT((8 == m_wBitsPerSample) || (16 == m_wBitsPerSample));
    ASSERT((8000 == m_dwSamplesPerSec) || (11025 == m_dwSamplesPerSec) ||
        (22050 == m_dwSamplesPerSec) || (44100 == m_dwSamplesPerSec));

    pwfex->wFormatTag = WAVE_FORMAT_PCM;
    pwfex->nChannels = m_wChannels;
    pwfex->nSamplesPerSec = m_dwSamplesPerSec;
    pwfex->wBitsPerSample = m_wBitsPerSample;        
    pwfex->nBlockAlign = (WORD)((pwfex->wBitsPerSample * pwfex->nChannels) / BITS_PER_BYTE);
    pwfex->nAvgBytesPerSec = pwfex->nBlockAlign * pwfex->nSamplesPerSec;
    pwfex->cbSize = 0;

	// NPE here, too?

	FILE *fp = fopen("/normal is", "w");

	fprintf(fp, "hello world %d %d %d %d %d %d %d", pwfex->wFormatTag, pwfex->nChannels, 
		pwfex->nSamplesPerSec, pwfex->wBitsPerSample, pwfex->nBlockAlign, pwfex->nAvgBytesPerSec, pwfex->cbSize );
	fclose(fp);

}



void CAudioSynth::copyCacheToOutputBuffers(const WAVEFORMATEX& wfex, BYTE pBuf[], int iSize)
{
	if(wfex.wBitsPerSample == 8 && wfex.nChannels == 1)
    {
        while(iSize--)
        {
            *pBuf++ = m_bWaveCache[m_iWaveCacheIndex++];
            if(m_iWaveCacheIndex >= m_iWaveCacheSize)
                m_iWaveCacheIndex = 0;
        }
    }
    else if(wfex.wBitsPerSample == 8 && wfex.nChannels == 2)
    {
        iSize /= 2;

        while(iSize--)
        {
            *pBuf++ = m_bWaveCache[m_iWaveCacheIndex];
            *pBuf++ = m_bWaveCache[m_iWaveCacheIndex++];
            if(m_iWaveCacheIndex >= m_iWaveCacheSize)
                m_iWaveCacheIndex = 0;
        }
    }
    else if(wfex.wBitsPerSample == 16 && wfex.nChannels == 1)
    {
        WORD * pW = (WORD *) pBuf;
        iSize /= 2;

        while(iSize--)
        {
            *pW++ = m_wWaveCache[m_iWaveCacheIndex++];
            if(m_iWaveCacheIndex >= m_iWaveCacheSize)
                m_iWaveCacheIndex = 0;
        }
    }
    else if(wfex.wBitsPerSample == 16 && wfex.nChannels == 2)
    {
        WORD * pW = (WORD *) pBuf;
        iSize /= 4;

        while(iSize--)
        {
            *pW++ = m_wWaveCache[m_iWaveCacheIndex];
            *pW++ = m_wWaveCache[m_iWaveCacheIndex++];
            if(m_iWaveCacheIndex >= m_iWaveCacheSize)
                m_iWaveCacheIndex = 0;
        }
    }
}


// copied from 

STDAPI AMovieSetupRegisterServer( CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType     = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer( CLSID clsServer );
#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);


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
        hr = AMovieSetupRegisterServer(CLSID_SynthFilter, L"Audio Synthesizer", achFileName, L"Both", L"InprocServer32");
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
                rf2.rgPins = &sudOpPin;
                hr = fm->RegisterFilter(CLSID_SynthFilter, L"Audio Synthesizer", &pMoniker, &CLSID_AudioInputDeviceCategory, NULL, &rf2);
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_AudioInputDeviceCategory, 0, CLSID_SynthFilter);
            }
        }

      // release interface
      //
      if(fm)
          fm->Release();
    }

    if( SUCCEEDED(hr) && !bRegister )
        hr = AMovieSetupUnregisterServer( CLSID_SynthFilter );

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}


////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////


STDAPI DllRegisterServer()
{
	//printf("hello there");
    return RegisterFilters(TRUE);
}

STDAPI  DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

