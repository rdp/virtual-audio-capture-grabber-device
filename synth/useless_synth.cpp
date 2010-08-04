

#include <windows.h>
#include <streams.h>

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


//
// put_SamplesPerSec
//
STDMETHODIMP CSynthFilter::put_SamplesPerSec(int SamplesPerSec) 
{
    // This function holds the state lock because it does not want
    // the filter's format type state or its' connection state
    // to change between the call to put_SamplesPerSec() and the call to
    // ReconnectWithNewFormat().
    CAutoLock lStateLock(pStateLock());

    HRESULT hr = m_Synth->put_SamplesPerSec(SamplesPerSec);
    if( FAILED( hr ) ) {
        return hr;
    }

    ReconnectWithNewFormat ();

    DbgLog((LOG_TRACE, 3, TEXT("put_SamplesPerSec: %d"), SamplesPerSec));
    return NOERROR;
}


//
// put_Amplitude
//
STDMETHODIMP CSynthFilter::put_Amplitude(int Amplitude)
{
    m_Synth->put_Amplitude (Amplitude);

    DbgLog((LOG_TRACE, 3, TEXT("put_Amplitude: %d"), Amplitude));
    return NOERROR;
}



//
// put_SweepRange
//
STDMETHODIMP CSynthFilter::put_SweepRange(int SweepStart, int SweepEnd) 
{
    m_Synth->put_SweepRange (SweepStart, SweepEnd);

    DbgLog((LOG_TRACE, 3, TEXT("put_SweepRange: %d %d"), SweepStart, SweepEnd));
    return NOERROR;
}


STDMETHODIMP CSynthFilter::put_OutputFormat(SYNTH_OUTPUT_FORMAT ofOutputFormat) 
{
    // This function holds the state lock because it does not want
    // the filter's format type state or its' connection state
    // to change between the call to put_OutputFormat() and the call to
    // ReconnectWithNewFormat().
    CAutoLock lStateLock(pStateLock());    

    HRESULT hr = m_Synth->put_OutputFormat(ofOutputFormat);
    if (FAILED(hr)) {
        return hr;
    }

    ReconnectWithNewFormat();
    return S_OK;
}


//
// put_BitsPerSample
//
STDMETHODIMP CSynthFilter::put_BitsPerSample(int BitsPerSample) 
{
    // This function holds the state lock because it does not want
    // the filter's format type state or its' connection state
    // to change between the call to put_BitsPerSample() and the call to
    // ReconnectWithNewFormat().
    CAutoLock lStateLock(pStateLock());

    HRESULT hr = m_Synth->put_BitsPerSample(BitsPerSample);
    if( FAILED( hr ) ) {
        return hr;
    }

    ReconnectWithNewFormat ();

    DbgLog((LOG_TRACE, 3, TEXT("put_BitsPerSample: %d"), BitsPerSample));
    return NOERROR;
}


//
// put_Channels
//
STDMETHODIMP CSynthFilter::put_Channels(int Channels) 
{
    // This function holds the state lock because it does not want
    // the filter's format type state or its' connection state
    // to change between the call to put_Channels() and the call to
    // ReconnectWithNewFormat().
    CAutoLock lStateLock(pStateLock());

    HRESULT hr = m_Synth->put_Channels(Channels);
    if( FAILED( hr ) ) {
        return hr;
    }

    ReconnectWithNewFormat ();

    DbgLog((LOG_TRACE, 3, TEXT("put_Channels: %d"), Channels));
    return NOERROR;
}


//
// CSynthFilter::Destructor
//
CSynthFilter::~CSynthFilter(void) 
{
    //
    //  Base class will free our pins
    //
}




//
// NonDelegatingQueryInterface
//
// Reveal our property page, persistance, and control interfaces

STDMETHODIMP CSynthFilter::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    if (riid == IID_ISynth2) {
        return GetInterface((ISynth2 *) this, ppv);
    }
    else if (riid == IID_IPersistStream) {
        return GetInterface((IPersistStream *) this, ppv);
    }
    else if (riid == IID_ISpecifyPropertyPages) {
        return GetInterface((ISpecifyPropertyPages *) this, ppv);
    } 
    else {
        return CDynamicSource::NonDelegatingQueryInterface(riid, ppv);
    }
}


//
// GetPages
//
STDMETHODIMP CSynthFilter::GetPages(CAUUID * pPages) 
{
    CheckPointer(pPages,E_POINTER);

    pPages->cElems = 1;
    pPages->pElems = (GUID *) CoTaskMemAlloc(sizeof(GUID));
    if (pPages->pElems == NULL) {
        return E_OUTOFMEMORY;
    }

    *(pPages->pElems) = CLSID_SynthPropertyPage;

    return NOERROR;
}



// -------------------------------------------------------------------------
// --- IPersistStream ---
// -------------------------------------------------------------------------

#define WRITEOUT(var)   hr = pStream->Write(&var, sizeof(var), NULL); \
                        if (FAILED(hr)) return hr;

#define READIN(var)     hr = pStream->Read(&var, sizeof(var), NULL); \
                        if (FAILED(hr)) return hr;



// appears this one is called by directsound...

HRESULT CSynthFilter::WriteToStream(IStream *pStream)
{
    CheckPointer(pStream,E_POINTER);

    HRESULT hr;
    int i, k;

    get_Frequency (&i);  
    WRITEOUT(i);

    get_Waveform (&i);
    WRITEOUT(i);

    get_Channels (&i);
    WRITEOUT(i);

    get_BitsPerSample (&i);
    WRITEOUT(i);

    get_SamplesPerSec (&i);
    WRITEOUT(i);

    get_Amplitude (&i);
    WRITEOUT(i);

    get_SweepRange (&i, &k);
    WRITEOUT(i);
    WRITEOUT(k);

	//#get_OutputFormat((SYNTH_OUTPUT_FORMAT*)&i);
	i = WAVE_FORMAT_PCM;
    WRITEOUT(i);

    return hr;
}

// I think this is just to read from the properties pages...I think...

HRESULT CSynthFilter::ReadFromStream(IStream *pStream)
{
    CheckPointer(pStream,E_POINTER);

    if (GetSoftwareVersion() != mPS_dwFileVersion)
        return E_FAIL;

    HRESULT hr;
    int i, k;

    READIN(i); // reads 4 bytes...
    put_Frequency(i);

    READIN(i);
    put_Waveform (i);

    READIN(i);
    put_Channels (i);

    READIN(i);
    put_BitsPerSample (i);

    READIN(i);
    put_SamplesPerSec (i);

    READIN(i);
    put_Amplitude (i);

    READIN(i);
    READIN(k);
    put_SweepRange (i, k);

    READIN(i);
    put_OutputFormat((SYNTH_OUTPUT_FORMAT)i);

    return hr;
}



//
// get_Frequency
//
STDMETHODIMP CAudioSynth::get_Frequency(int *pFrequency)
{
    CheckPointer(pFrequency,E_POINTER);

    *pFrequency = m_iFrequency;

    DbgLog((LOG_TRACE, 3, TEXT("get_Frequency: %d"), *pFrequency));
    return NOERROR;
}


//
// put_Frequency
//
STDMETHODIMP CAudioSynth::put_Frequency(int Frequency)
{
    CAutoLock l(m_pStateLock);

    m_iFrequency = Frequency;

    DbgLog((LOG_TRACE, 3, TEXT("put_Frequency: %d"), Frequency));
    return NOERROR;
}

//
// get_Waveform
//
STDMETHODIMP CAudioSynth::get_Waveform(int *pWaveform)
{
    CheckPointer(pWaveform,E_POINTER);
    *pWaveform = m_iWaveform;

    DbgLog((LOG_TRACE, 3, TEXT("get_Waveform: %d"), *pWaveform));
    return NOERROR;
}


//
// put_Waveform
//
STDMETHODIMP CAudioSynth::put_Waveform(int Waveform)
{
    CAutoLock l(m_pStateLock);

    m_iWaveform = Waveform;

    DbgLog((LOG_TRACE, 3, TEXT("put_Waveform: %d"), Waveform));
    return NOERROR;
}


//
// get_Channels
//
STDMETHODIMP CAudioSynth::get_Channels(int *pChannels)
{
    CheckPointer(pChannels,E_POINTER);

    *pChannels = m_wChannels;

    DbgLog((LOG_TRACE, 3, TEXT("get_Channels: %d"), *pChannels));
    return NOERROR;
}


//
// put_Channels
//
STDMETHODIMP CAudioSynth::put_Channels(int Channels)
{
    CAutoLock l(m_pStateLock);

    m_wChannels = (WORD) Channels;
    return NOERROR;
}


//
// get_BitsPerSample
//
STDMETHODIMP CAudioSynth::get_BitsPerSample(int *pBitsPerSample)
{
    CheckPointer(pBitsPerSample,E_POINTER);

    *pBitsPerSample = m_wBitsPerSample;

    DbgLog((LOG_TRACE, 3, TEXT("get_BitsPerSample: %d"), *pBitsPerSample));
    return NOERROR;
}


//
// put_BitsPerSample
//
STDMETHODIMP CAudioSynth::put_BitsPerSample(int BitsPerSample)
{
    CAutoLock l(m_pStateLock);

    m_wBitsPerSample = (WORD) BitsPerSample;
    return NOERROR;
}


//
// get_SamplesPerSec
//
STDMETHODIMP CAudioSynth::get_SamplesPerSec(int *pSamplesPerSec)
{
    CheckPointer(pSamplesPerSec,E_POINTER);

    *pSamplesPerSec = m_dwSamplesPerSec;

    DbgLog((LOG_TRACE, 3, TEXT("get_SamplesPerSec: %d"), *pSamplesPerSec));
    return NOERROR;
}


//
// put_SamplesPerSec
//
STDMETHODIMP CAudioSynth::put_SamplesPerSec(int SamplesPerSec)
{
    CAutoLock l(m_pStateLock);

    m_dwSamplesPerSec = SamplesPerSec;
    return NOERROR;
}


//
// put_SynthFormat
//
STDMETHODIMP CAudioSynth::put_SynthFormat(int Channels, int BitsPerSample,
                                          int SamplesPerSec)
{
    CAutoLock l(m_pStateLock);

    m_wChannels = (WORD) Channels;
    m_wBitsPerSample = (WORD) BitsPerSample;
    m_dwSamplesPerSec = SamplesPerSec;

    DbgLog((LOG_TRACE, 1, TEXT("put_SynthFormat: %d-bit %d-channel %dHz"),
        BitsPerSample, Channels, SamplesPerSec));

    return NOERROR;
}


//
// get_Amplitude
//
STDMETHODIMP CAudioSynth::get_Amplitude(int *pAmplitude)
{
    CheckPointer(pAmplitude,E_POINTER);

    *pAmplitude =  m_iAmplitude;

    DbgLog((LOG_TRACE, 3, TEXT("get_Amplitude: %d"), *pAmplitude));
    return NOERROR;
}


//
// put_Amplitude
//
STDMETHODIMP CAudioSynth::put_Amplitude(int Amplitude)
{
    CAutoLock l(m_pStateLock);

    if(Amplitude > MaxAmplitude || Amplitude < MinAmplitude)
        return E_INVALIDARG;

    m_iAmplitude = Amplitude;

    DbgLog((LOG_TRACE, 3, TEXT("put_Amplitude: %d"), Amplitude));
    return NOERROR;
}


//
// get_SweepRange
//
STDMETHODIMP CAudioSynth::get_SweepRange(int *pSweepStart, int *pSweepEnd)
{
    CheckPointer(pSweepStart,E_POINTER);
    CheckPointer(pSweepEnd,E_POINTER);

    *pSweepStart = m_iSweepStart;
    *pSweepEnd   = m_iSweepEnd;

    DbgLog((LOG_TRACE, 3, TEXT("get_SweepStart: %d %d"), *pSweepStart, *pSweepEnd));
    return NOERROR;
}


//
// put_SweepRange
//
STDMETHODIMP CAudioSynth::put_SweepRange(int SweepStart, int SweepEnd)
{
    CAutoLock l(m_pStateLock);

    m_iSweepStart = SweepStart;
    m_iSweepEnd = SweepEnd;

    DbgLog((LOG_TRACE, 3, TEXT("put_SweepRange: %d %d"), SweepStart, SweepEnd));
    return NOERROR;
}


//
// get_OutputFormat
//
STDMETHODIMP CAudioSynth::get_OutputFormat(SYNTH_OUTPUT_FORMAT *pOutputFormat)
{
    CheckPointer(pOutputFormat, E_POINTER);

    ValidateReadWritePtr(pOutputFormat, sizeof(SYNTH_OUTPUT_FORMAT));

    switch(m_wFormatTag)
    {
        case WAVE_FORMAT_PCM:
            *pOutputFormat = SYNTH_OF_PCM;
            break;

        case WAVE_FORMAT_ADPCM:
            *pOutputFormat = SYNTH_OF_MS_ADPCM;
            break;

        default:
            return E_UNEXPECTED;
    }

    return S_OK;
}


//
// put_OutputFormat
//
STDMETHODIMP CAudioSynth::put_OutputFormat(SYNTH_OUTPUT_FORMAT ofOutputFormat)
{
    CAutoLock l(m_pStateLock);    

    switch(ofOutputFormat)
    {
        case SYNTH_OF_PCM:
            m_wFormatTag = WAVE_FORMAT_PCM;
            break;

        case SYNTH_OF_MS_ADPCM:
            m_wFormatTag = WAVE_FORMAT_ADPCM;
            break;

        default:
            return E_INVALIDARG;
    }

    return S_OK;
}
