

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


HRESULT LoopbackCapture(const WAVEFORMATEX& wfex, BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly);

//
// FillAudioBuffer
//
// This actually fills it with the sin wave by copying it verbatim into the output...
//
void CAudioSynth::FillPCMAudioBuffer(const WAVEFORMATEX& wfex, BYTE pBuf[], int iSize)
{
    BOOL fCalcCache = FALSE;

    // The caller should always hold the state lock because this
    // function uses m_iFrequency,  m_iFrequencyLast, m_iWaveform
    // m_iWaveformLast, m_iAmplitude, m_iAmplitudeLast, m_iWaveCacheIndex
    // m_iWaveCacheSize, m_bWaveCache and m_wWaveCache.  The caller should
    // also hold the state lock because this function calls CalcCache().
    ASSERT(CritCheckIn(m_pStateLock));

    // Only realloc the cache if the format has changed !
    if(m_iFrequency != m_iFrequencyLast)
    {
        fCalcCache = TRUE;
        m_iFrequencyLast = m_iFrequency;
    }
    if(m_iWaveform != m_iWaveformLast)
    {
        fCalcCache = TRUE;
        m_iWaveformLast = m_iWaveform;
    }
    if(m_iAmplitude != m_iAmplitudeLast)
    {
        fCalcCache = TRUE;
        m_iAmplitudeLast = m_iAmplitude;
    }

    if(fCalcCache)
    {
		// recalculate the sin wave...
        CalcCache(wfex);
    }

	  // sin wave (old way)
      // Copy cache to output buffers
      copyCacheToOutputBuffers(wfex, pBuf, iSize);
}

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



// -------------------------------------------------------------------------
// ISynth2, the control interface for the synthesizer
// -------------------------------------------------------------------------

//
// get_Frequency
//
STDMETHODIMP CSynthFilter::get_Frequency(int *Frequency) 
{
    m_Synth->get_Frequency(Frequency);

    DbgLog((LOG_TRACE, 3, TEXT("get_Frequency: %d"), *Frequency));
    return NOERROR;
}


//
// put_Frequency
//
STDMETHODIMP CSynthFilter::put_Frequency(int Frequency) 
{
    m_Synth->put_Frequency (Frequency);

    DbgLog((LOG_TRACE, 3, TEXT("put_Frequency: %d"), Frequency));
    return NOERROR;
}


//
// get_Waveform
//
STDMETHODIMP CSynthFilter::get_Waveform(int *Waveform) 
{
    m_Synth->get_Waveform (Waveform);

    DbgLog((LOG_TRACE, 3, TEXT("get_Waveform: %d"), *Waveform));
    return NOERROR;
}


//
// put_Waveform
//
STDMETHODIMP CSynthFilter::put_Waveform(int Waveform) 
{
    m_Synth->put_Waveform (Waveform);

    DbgLog((LOG_TRACE, 3, TEXT("put_Waveform: %d"), Waveform));
    return NOERROR;
}


//
// get_Channels
//
STDMETHODIMP CSynthFilter::get_Channels(int *Channels) 
{
    HRESULT hr = m_Synth->get_Channels( Channels );

    DbgLog((LOG_TRACE, 3, TEXT("get_Channels: %d"), *Channels));
    return hr;
}


//
// If the format changes, we need to reconnect
//
void CSynthFilter::ReconnectWithNewFormat(void) 
{
    // The caller must hold the state lock because this
    // function calls IsConnected().
    ASSERT(CritCheckIn(pStateLock()));

    // The synth filter's only has one pin.  The pin is an output pin.
    CDynamicSourceStream* pOutputPin = (CDynamicSourceStream*)GetPin(0);

    if( pOutputPin->IsConnected() ) {
        pOutputPin->OutputPinNeedsToBeReconnected();
    }
}



//
// get_BitsPerSample
//
STDMETHODIMP CSynthFilter::get_BitsPerSample(int *BitsPerSample) 
{
    HRESULT hr = m_Synth->get_BitsPerSample(BitsPerSample);

    DbgLog((LOG_TRACE, 3, TEXT("get_BitsPerSample: %d"), *BitsPerSample));
    return hr;
}



//
// get_SamplesPerSec
//
STDMETHODIMP CSynthFilter::get_SamplesPerSec(int *SamplesPerSec) 
{
    HRESULT hr = m_Synth->get_SamplesPerSec(SamplesPerSec);
    
    DbgLog((LOG_TRACE, 3, TEXT("get_SamplesPerSec: %d"), *SamplesPerSec));
    return hr;
}



//
// get_Amplitude
//
STDMETHODIMP CSynthFilter::get_Amplitude(int *Amplitude) 
{
    m_Synth->get_Amplitude (Amplitude);

    DbgLog((LOG_TRACE, 3, TEXT("get_Amplitude: %d"), *Amplitude));
    return NOERROR;
}



//
// get_SweepRange
//
STDMETHODIMP CSynthFilter::get_SweepRange(int *SweepStart, int *SweepEnd) 
{
    m_Synth->get_SweepRange (SweepStart, SweepEnd);

    DbgLog((LOG_TRACE, 3, TEXT("get_SweepStart: %d %d"), *SweepStart, *SweepEnd));
    return NOERROR;
}


STDMETHODIMP CSynthFilter::get_OutputFormat(SYNTH_OUTPUT_FORMAT* pOutputFormat) 
{
    HRESULT hr = m_Synth->get_OutputFormat(pOutputFormat);
    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

void CSynthStream::DerivePCMFormatFromADPCMFormatStructure(const WAVEFORMATEX& wfexADPCM, 
                                                           WAVEFORMATEX* pwfexPCM)
{
    ASSERT(pwfexPCM);
    if (!pwfexPCM)
        return;

    pwfexPCM->wFormatTag = WAVE_FORMAT_PCM; 
    pwfexPCM->wBitsPerSample = 16;
    pwfexPCM->cbSize = 0;

    pwfexPCM->nChannels = wfexADPCM.nChannels;
    pwfexPCM->nSamplesPerSec = wfexADPCM.nSamplesPerSec;

    pwfexPCM->nBlockAlign = (WORD)((pwfexPCM->nChannels * pwfexPCM->wBitsPerSample) / BITS_PER_BYTE);
    pwfexPCM->nAvgBytesPerSec = pwfexPCM->nBlockAlign * pwfexPCM->nSamplesPerSec;
}


void CAudioSynth::CalcCache(const WAVEFORMATEX& wfex)
{
    switch(m_iWaveform)
    {
        case WAVE_SINE:
            CalcCacheSine(wfex);
            break;

        case WAVE_SQUARE:
            CalcCacheSquare(wfex);
            break;

        case WAVE_SAWTOOTH:
            CalcCacheSawtooth(wfex);
            break;

        case WAVE_SINESWEEP:
            CalcCacheSweep(wfex);
            break;
    }
}


//
// CalcCacheSine
//
//
void CAudioSynth::CalcCacheSine(const WAVEFORMATEX& wfex)
{
    int i;
    double d;
    double amplitude;
    double FTwoPIDivSpS;

    amplitude = ((wfex.wBitsPerSample == 8) ? 127 : 32767 ) * m_iAmplitude / 100;

    FTwoPIDivSpS = m_iFrequency * TWOPI / wfex.nSamplesPerSec;

    m_iWaveCacheIndex = 0;
    m_iCurrentSample = 0;

    if(wfex.wBitsPerSample == 8)
    {
        BYTE * pB = m_bWaveCache;

        for(i = 0; i < m_iWaveCacheSize; i++)
        {
            d = FTwoPIDivSpS * i;
            *pB++ = (BYTE) ((sin(d) * amplitude) + 128);
        }
    }
    else
    {
        PWORD pW = (PWORD) m_wWaveCache;

        for(i = 0; i < m_iWaveCacheSize; i++)
        {
            d = FTwoPIDivSpS * i;
            *pW++ = (WORD) (sin(d) * amplitude);
        }
    }
}


//
// CalcCacheSquare
//
//
void CAudioSynth::CalcCacheSquare(const WAVEFORMATEX& wfex)
{
    int i;
    double d;
    double FTwoPIDivSpS;
    BYTE b0, b1;
    WORD w0, w1;

    b0 = (BYTE) (128 - (127 * m_iAmplitude / 100));
    b1 = (BYTE) (128 + (127 * m_iAmplitude / 100));
    w0 = (WORD) (32767. * m_iAmplitude / 100);
    w1 = (WORD) - (32767. * m_iAmplitude / 100);

    FTwoPIDivSpS = m_iFrequency * TWOPI / wfex.nSamplesPerSec;

    m_iWaveCacheIndex = 0;
    m_iCurrentSample = 0;

    if(wfex.wBitsPerSample == 8)
    {
        BYTE * pB = m_bWaveCache;

        for(i = 0; i < m_iWaveCacheSize; i++)
        {
            d = FTwoPIDivSpS * i;
            *pB++ = (BYTE) ((sin(d) >= 0) ? b1 : b0);
        }
    }
    else
    {
        PWORD pW = (PWORD) m_wWaveCache;

        for(i = 0; i < m_iWaveCacheSize; i++)
        {
            d = FTwoPIDivSpS * i;
            *pW++ = (WORD) ((sin(d) >= 0) ? w1 : w0);
        }
    }
}


//
// CalcCacheSawtooth
//
void CAudioSynth::CalcCacheSawtooth(const WAVEFORMATEX& wfex)
{
    int i;
    double d;
    double amplitude;
    double FTwoPIDivSpS;
    double step;
    double curstep=0;
    BOOL fLastWasNeg = TRUE;
    BOOL fPositive;

    amplitude = ((wfex.wBitsPerSample == 8) ? 255 : 65535 )
        * m_iAmplitude / 100;

    FTwoPIDivSpS = m_iFrequency * TWOPI / wfex.nSamplesPerSec;
    step = amplitude * m_iFrequency / wfex.nSamplesPerSec;

    m_iWaveCacheIndex = 0;
    m_iCurrentSample = 0;

    BYTE * pB = m_bWaveCache;
    PWORD pW = (PWORD) m_wWaveCache;

    for(i = 0; i < m_iWaveCacheSize; i++)
    {
        d = FTwoPIDivSpS * i;

        // OneShot triggered on positive zero crossing
        fPositive = (sin(d) >= 0);

        if(fLastWasNeg && fPositive)
        {
            if(wfex.wBitsPerSample == 8)
                curstep = 128 - amplitude / 2;
            else
                curstep = 32768 - amplitude / 2;
        }
        fLastWasNeg = !fPositive;

        if(wfex.wBitsPerSample == 8)
            *pB++ = (BYTE) curstep;
        else
            *pW++ = (WORD) (-32767 + curstep);

        curstep += step;
    }
}


//
// CalcCacheSweep
//
void CAudioSynth::CalcCacheSweep(const WAVEFORMATEX& wfex)
{
    int i;
    double d;
    double amplitude;
    double FTwoPIDivSpS;
    double CurrentFreq;
    double DeltaFreq;

    amplitude = ((wfex.wBitsPerSample == 8) ? 127 : 32767 ) * m_iAmplitude / 100;

    DeltaFreq = ((double) m_iSweepEnd - m_iSweepStart) / m_iWaveCacheSize;
    CurrentFreq = m_iSweepStart;

    m_iWaveCacheIndex = 0;
    m_iCurrentSample = 0;

    if(wfex.wBitsPerSample == 8)
    {
        BYTE * pB = m_bWaveCache;
        d = 0.0;

        for(i = 0; i < m_iWaveCacheSize; i++)
        {
            FTwoPIDivSpS = (int) CurrentFreq * TWOPI / wfex.nSamplesPerSec;
            CurrentFreq += DeltaFreq;
            d += FTwoPIDivSpS;
            *pB++ = (BYTE) ((sin(d) * amplitude) + 128);
        }
    }
    else
    {
        PWORD pW = (PWORD) m_wWaveCache;
        d = 0.0;

        for(i = 0; i < m_iWaveCacheSize; i++)
        {
            FTwoPIDivSpS = (int) CurrentFreq * TWOPI / wfex.nSamplesPerSec;
            CurrentFreq += DeltaFreq;
            d += FTwoPIDivSpS;
            *pW++ = (WORD) (sin(d) * amplitude);
        }
    }
}

