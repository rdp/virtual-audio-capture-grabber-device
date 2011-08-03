#include "stdafx.h"

#include <streams.h>
#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>
#include "common.h"
#include "assert.h"
#include <memory.h>

//HRESULT open_file(LPCWSTR szFileName, HMMIO *phFile);

HRESULT get_default_device(IMMDevice **ppMMDevice);

IAudioCaptureClient *pAudioCaptureClient;
IAudioClient *pAudioClient;
HANDLE hTask;
bool bFirstPacket = true;
IMMDevice *m_pMMDevice;
UINT32 nBlockAlign;
UINT32 pnFrames;

CCritSec csMyLock;  // Critical section starts not locked...

int shouldStop = 0;

BYTE pBufLocal[1024*1024]; // 1MB is quite awhile I think... TODO check overflow...
long pBufLocalSize = 1024*1024;// TODO needed?
long pBufLocalCurrentEndLocation = 0;

long expectedMaxBufferSize;

void setExpectedMaxBufferSize(long toThis) {
	expectedMaxBufferSize = toThis;
}
HANDLE m_hThread;

static DWORD WINAPI propagateBufferForever(LPVOID pv);

// we only call this once...
HRESULT LoopbackCaptureSetup()
{
	pnFrames = 0;
	bool bInt16 = true; // makes it actually work, for some reason...LODO
	
    HRESULT hr;
    hr = get_default_device(&m_pMMDevice); // so it can re-place our pointer...
    if (FAILED(hr)) {
        return hr;
    }

    // activate an (the default, for us) IAudioClient
    hr = m_pMMDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL, NULL,
        (void**)&pAudioClient
    );
    if (FAILED(hr)) {
        printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        return hr;
    }
    
    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr)) {
        printf("IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
        pAudioClient->Release();
        return hr;
    }

    // get the default device format (incoming...)
    WAVEFORMATEX *pwfx; // incoming wave...
	// apparently propogated only by GetMixFormat...
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        printf("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        return hr;
    }

    if (bInt16) {
        // coerce int-16 wave format
        // can do this in-place since we're not changing the size of the format
        // also, the engine will auto-convert from float to int for us
        switch (pwfx->wFormatTag) {
            case WAVE_FORMAT_IEEE_FLOAT:
                pwfx->wFormatTag = WAVE_FORMAT_PCM;
                pwfx->wBitsPerSample = 16;
                pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
                pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
                break;

            case WAVE_FORMAT_EXTENSIBLE:
                {
                    // naked scope for case-local variable
                    PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
                    if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
						// WE GET HERE!
                        pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                        pEx->Samples.wValidBitsPerSample = 16;
                        pwfx->wBitsPerSample = 16;
                        pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
                        pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
						/* scawah lodo...
						if(ifNotNullThenJustSetTypeOnly) {
							PWAVEFORMATEXTENSIBLE pEx2 = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(ifNotNullThenJustSetTypeOnly);
							pEx2->SubFormat = pEx->SubFormat;
							pEx2->Samples.wValidBitsPerSample = pEx->Samples.wValidBitsPerSample;
						} */
                    } else {
                        printf("Don't know how to coerce mix format to int-16\n");
                        CoTaskMemFree(pwfx);
                        pAudioClient->Release();
                        return E_UNEXPECTED;
                    }
                }
                break;

            default:
                printf("Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16\n", pwfx->wFormatTag);
                CoTaskMemFree(pwfx);
                pAudioClient->Release();
                return E_UNEXPECTED;
        }
    }
	/* scawah part
	if(ifNotNullThenJustSetTypeOnly) {
		// pwfx is set at this point...
		WAVEFORMATEX* pwfex = ifNotNullThenJustSetTypeOnly;
		// copy them all out as the possible format...hmm...


                pwfx->wFormatTag = WAVE_FORMAT_PCM;
                pwfx->wBitsPerSample = 16;
                pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
                pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;


		pwfex->wFormatTag = pwfx->wFormatTag;
		pwfex->nChannels = pwfx->nChannels;
        pwfex->nSamplesPerSec = pwfx->nSamplesPerSec;
        pwfex->wBitsPerSample = pwfx->wBitsPerSample;
        pwfex->nBlockAlign = pwfx->nBlockAlign;
        pwfex->nAvgBytesPerSec = pwfx->nAvgBytesPerSec;
        pwfex->cbSize = pwfx->cbSize;
		//FILE *fp = fopen("/normal2", "w"); // fails on me? maybe juts a VLC thing...
		//fprintf(fp, "hello world %d %d %d %d %d %d %d", pwfex->wFormatTag, pwfex->nChannels, 
		//	pwfex->nSamplesPerSec, pwfex->wBitsPerSample, pwfex->nBlockAlign, pwfex->nAvgBytesPerSec, pwfex->cbSize );
		//fclose(fp);
		// cleanup
		// I might be leaking here...
		CoTaskMemFree(pwfx);
        pAudioClient->Release();
        //m_pMMDevice->Release();
		return hr;
	}*/

    MMCKINFO ckRIFF = {0};
    MMCKINFO ckData = {0};

    nBlockAlign = pwfx->nBlockAlign;
    
    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop...
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, 0
    );
    if (FAILED(hr)) {
        printf("IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
        pAudioClient->Release();
        return hr;
    }
    CoTaskMemFree(pwfx);

    // activate an IAudioCaptureClient
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pAudioCaptureClient // CARE INSTANTIATION
    );
    if (FAILED(hr)) {
        printf("IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
        //CloseHandle(hWakeUp);
        pAudioClient->Release();
        return hr;
    }
    
    // register with MMCSS
    DWORD nTaskIndex = 0;
    hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
    if (NULL == hTask) {
        DWORD dwErr = GetLastError();
        printf("AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
        pAudioCaptureClient->Release();
        //CloseHandle(hWakeUp);
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }    

    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        printf("IAudioClient::Start failed: hr = 0x%08x\n", hr);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        pAudioClient->Release();
        return hr;
    }
    
    bFirstPacket = true;

	// start the grabbing thread...hmm...
	DWORD dwThreadID;
    m_hThread = CreateThread(NULL,
                            0,
                            propagateBufferForever,
                            0,
                            0,
                            &dwThreadID);
    if(!m_hThread)
    {
        DWORD dwErr = GetLastError();
        return HRESULT_FROM_WIN32(dwErr);
    }

	return hr;

} // end LoopbackCaptureSetup



// LODO cleanups..
/*
// stop the thread and close the handle
HRESULT
CAsyncIo::CloseThread(void)
{
    // signal the thread-exit object
    m_evStop.Set();

    if(m_hThread)
    {
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
        m_hThread = NULL;
    }

    return S_OK;
}*/

HRESULT propagateBufferOnce();
static DWORD WINAPI propagateBufferForever(LPVOID pv) {
  while(!shouldStop) {
    HRESULT hr = propagateBufferOnce();
	if(FAILED(hr)) {
	 return hr; // TODO can exit...
	}
  }
  return S_OK;
}

HRESULT propagateBufferOnce() {
	HRESULT hr = S_OK;

	// this should also...umm...detect the timeout stuff and fake fill?
	// TODO timing...

    // grab a chunk...
	int gotAnyAtAll = FALSE;
	DWORD start_time = timeGetTime();
	INT32 nBytesWrote = 0; // LODO remove this nBytesWrote
    while (true) {
        UINT32 nNextPacketSize;
        hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize); // get next packet, if one is ready...
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x\n", nBytesWrote, pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;
        }

        if (0 == nNextPacketSize) {
            // no data yet, we're either waiting between incoming chunks, or...no sound is being played on the computer currently <sigh>...
			// maybe I don't really...need to worry about this in the end, once I can figure out the timing stuffs? <sniff>
			DWORD millis_to_fill = (DWORD) (1.0/SECOND_FRACTIONS_TO_GRAB*1000); // truncate is ok :)
			assert(millis_to_fill > 1); // actually, we kind of lose precision/timing here, don't we...hmm...LODO with correct timing info...
			DWORD current_time = timeGetTime();
			if((current_time - start_time > millis_to_fill)) {
				if(!gotAnyAtAll) {
				  // after a full slice of apparent silence, punt and return fake silence! [to not confuse our downstream friends]
   			     {
                   CAutoLock cObjectLock(&csMyLock);  // Lock the critical section, releases scope after method is over with...
				   memset(pBufLocal, 0, expectedMaxBufferSize); // guess that's silent
    			   pBufLocalCurrentEndLocation = expectedMaxBufferSize; // LODO does this work at all?
				 }
	  			  return S_OK;
				} else {
				  assert(false); // want to know if this ever happens...
				}
			} else {
			  Sleep(1);
			  continue;
			}
        } else {
			gotAnyAtAll = TRUE;
		}

        // get the captured data
        BYTE *pData;
        UINT32 nNumFramesToRead;
        DWORD dwFlags;

		// I guess it gives us...umm...as much as possible?

        hr = pAudioCaptureClient->GetBuffer(
            &pData,
            &nNumFramesToRead,
            &dwFlags,
            NULL,
            NULL
        ); // ACTUALLY GET THE BUFFER which I assume it reads in the format of the fella we passed in
        // so...it reads nNumFrames and calls it good or what?
        
        
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nBytesWrote, pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;            
        }

        if (bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
            printf("Probably spurious glitch reported on first packet\n");
        } else if (AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY != dwFlags) {
			if(dwFlags != 0) {
		      // expected if audio turns on and off...
				// LODO make this a non sync point ...
              printf("IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, nBytesWrote, pnFrames);
              /*pAudioClient->Stop();
              AvRevertMmThreadCharacteristics(hTask);
              pAudioCaptureClient->Release(); // WE GET HERE			  
              pAudioClient->Release();            
              return E_UNEXPECTED;*/
			}
        }

        if (0 == nNumFramesToRead) {
            printf("IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", nBytesWrote, pnFrames);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return E_UNEXPECTED;            
        }

		pnFrames += nNumFramesToRead; // increment total count...		

        LONG lBytesToWrite = nNumFramesToRead * nBlockAlign; // nBlockAlign is "audio block size" or frame size.
		UINT i; // avoid some overflow...
		{
          CAutoLock cObjectLock(&csMyLock);  // Lock the critical section, releases scope after method is over with...
		  for(i = 0; i < lBytesToWrite && nBytesWrote < pBufLocalSize; i++) {
			pBufLocal[pBufLocalCurrentEndLocation++] = pData[i]; // lodo use a straight call... [?] if memcpy is faster... [?]
		  }
		}
        
        hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nBytesWrote, pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;            
        }
        
        bFirstPacket = false;
		return hr;
    } // capture something, anything! loop...

}


// iSize is max size of the BYTE buffer...so maybe...we should just drop it if we have past that size? hmm...
HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* totalBytesWrote)
 {
	while(true) {
       {
        CAutoLock cObjectLock(&csMyLock);  // Lock the critical section, releases scope after method is over with...
		if(pBufLocalCurrentEndLocation > 0) {
		  assert(pBufLocalCurrentEndLocation <= expectedMaxBufferSize);
  	      memcpy(pBuf, pBufLocal, pBufLocalCurrentEndLocation);
          *totalBytesWrote = pBufLocalCurrentEndLocation;
		  pBufLocalCurrentEndLocation = 0;
          return S_OK;
		} // else fall through to sleep
	  }
	  // sleep outside the lock ...
      Sleep(1); // LODO ??
	}
}

void loopbackRelease() {
	shouldStop = 1;
	WaitForSingleObject(m_hThread, INFINITE);
    CloseHandle(m_hThread);
    m_hThread = NULL;	
	pAudioClient->Stop();
    AvRevertMmThreadCharacteristics(hTask);
    pAudioCaptureClient->Release();
    pAudioClient->Release();
    m_pMMDevice->Release();
}

