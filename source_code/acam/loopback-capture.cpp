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
void outputStats();

IAudioCaptureClient *pAudioCaptureClient;
IAudioClient *pAudioClient;
HANDLE hTask;
bool bFirstPacket = true;
IMMDevice *m_pMMDevice;
UINT32 nBlockAlign;
UINT32 pnFrames;

CCritSec csMyLock;  // shared critical section. Starts not locked...

int shouldStop = 0;

BYTE pBufLocal[1024*1024]; // 1MB is quite awhile I think...
long pBufLocalSize = 1024*1024; // TODO needed?
long pBufLocalCurrentEndLocation = 0;

long expectedMaxBufferSize = 0;

void setExpectedMaxBufferSize(long toThis) {
  expectedMaxBufferSize = toThis;
}

HANDLE m_hThread;

static DWORD WINAPI propagateBufferForever(LPVOID pv);


#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { return hres; }
#define REFTIMES_PER_SEC  10000000


const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);


// we only call this once...per hit of the play button :)
HRESULT LoopbackCaptureSetup()
{
	shouldStop = false; // allow graphs to restart, if they so desire...
	pnFrames = 0;
	bool bInt16 = true; // makes it actually work, for some reason...LODO
	
    HRESULT hr;
    hr = get_default_device(&m_pMMDevice); // so it can re-place our pointer...
    if (FAILED(hr)) {
        return hr;
    }

	// tell it to not overflow one buffer's worth <sigh> not sure if this is right or not, and thus we don't "cache" or "buffer" more than that much currently...
	// but a buffer size is a buffer size...hmm...as long as we keep it small though...
	assert(expectedMaxBufferSize <= pBufLocalSize);
    // activate an (the default, for us) IAudioClient
    hr = m_pMMDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL, NULL,
        (void**)&pAudioClient
    );
    if (FAILED(hr)) {
        ShowOutput("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        return hr;
    }
    
    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr)) {
        ShowOutput("IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
        pAudioClient->Release();
        return hr;
    }

    // get the default device format (incoming...)
    WAVEFORMATEX *pwfx; // incoming wave...
	// apparently propogated only by GetMixFormat...
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        ShowOutput("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
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
                        ShowOutput("Don't know how to coerce mix format to int-16\n");
                        CoTaskMemFree(pwfx);
                        pAudioClient->Release();
                        return E_UNEXPECTED;
                    }
                }
                break;

            default:
                ShowOutput("Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16\n", pwfx->wFormatTag);
                CoTaskMemFree(pwfx);
                pAudioClient->Release();
                return E_UNEXPECTED;
        }
    }
	/* scawah setting stream types up to match...didn't seem to work well...

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
		//fShowOutput(fp, "hello world %d %d %d %d %d %d %d", pwfex->wFormatTag, pwfex->nChannels, 
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
    

// avoid stuttering on close
// http://social.msdn.microsoft.com/forums/en-US/windowspro-audiodevelopment/thread/c7ba0a04-46ce-43ff-ad15-ce8932c00171/ 
	
//IAudioClient *pAudioClient = NULL;
//IAudioCaptureClient *pCaptureClient = NULL;
IMMDeviceEnumerator *pEnumerator = NULL;
IMMDevice *pDevice = NULL;

IAudioRenderClient *pRenderClient = NULL;
WAVEFORMATEXTENSIBLE *captureDataFormat = NULL;
BYTE *captureData;

    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->GetMixFormat((WAVEFORMATEX **)&captureDataFormat);
    EXIT_ON_ERROR(hr)

	
    // initialise in sharedmode [this is the "silence" bug fella]
    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0,
					     REFTIMES_PER_SEC, // buffer size a full 1.0s, though prolly doesn't matter here.
                         0,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // get the frame count
    UINT32  bufferFrameCount;
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

    // create a render client
    hr = pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
    EXIT_ON_ERROR(hr)

    // get the buffer
    hr = pRenderClient->GetBuffer(bufferFrameCount, &captureData);
    EXIT_ON_ERROR(hr)

    // release it
    hr = pRenderClient->ReleaseBuffer(bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
    EXIT_ON_ERROR(hr)

    // release the audio client
    pAudioClient->Release();
    EXIT_ON_ERROR(hr)


    // create a new IAudioClient
    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)


    // -============================ now the sniffing code initialization stuff, direct from mauritius... ===================================

	// call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop...
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        REFTIMES_PER_SEC, // buffer size a full 1.0s
		0, pwfx, 0
    );
    if (FAILED(hr)) {
        ShowOutput("IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
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
        ShowOutput("IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
        pAudioClient->Release();
        return hr;
    }
    
    // register with MMCSS
    DWORD nTaskIndex = 0;
    hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
    if (NULL == hTask) {
        DWORD dwErr = GetLastError();
        ShowOutput("AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
        pAudioCaptureClient->Release();
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }    

    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        ShowOutput("IAudioClient::Start failed: hr = 0x%08x\n", hr);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        pAudioClient->Release();
        return hr;
    }
    
    bFirstPacket = true;

	// start the forever grabbing thread...
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
    } else {
		// we...shouldn't need this...maybe?
		// seems to make no difference...
		hr = SetThreadPriority(m_hThread, THREAD_PRIORITY_TIME_CRITICAL);
        if (FAILED(hr)) { // of course we always want to be a high prio thread, right? [we don't use much cpu...]
		  return hr;
  	    }
	}

	return hr;
} // end LoopbackCaptureSetup


HRESULT propagateBufferOnce();
static DWORD WINAPI propagateBufferForever(LPVOID pv) {
  while(!shouldStop) {
    HRESULT hr = propagateBufferOnce();
	if(FAILED(hr)) {
	 return hr;
	}
  }
  return S_OK;
}

extern CCritSec m_cSharedState;

int totalSuccessFullyread = 0;
int totalBlips = 0;
int totalOverflows = 0;

HRESULT propagateBufferOnce() {
	HRESULT hr = S_OK;

    // grab next audio chunk...
	int gotAnyAtAll = FALSE;
	DWORD start_time = timeGetTime();
    while (!shouldStop) {
        UINT32 nNextPacketSize;
        hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize); // get next packet, if one is ready...
        if (FAILED(hr)) {
            ShowOutput("IAudioCaptureClient::GetNextPacketSize failed after %u frames: hr = 0x%08x\n", pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;
        }

        if (0 == nNextPacketSize) {
            // no data yet, we're waiting, between incoming chunks of audio. [occurs even with silence on the line--it just means no new data yet]

			DWORD millis_to_fill = (DWORD) (1.0/SECOND_FRACTIONS_TO_GRAB*1000); // truncate is ok :) -- 1s
			assert(millis_to_fill > 1); // sanity
			DWORD current_time = timeGetTime();
			if((current_time - start_time > millis_to_fill)) {
				// I don't think we ever get to here anymore...thankfully, since it's mostly broken code probably, anyway
				if(!gotAnyAtAll) {
				  assert(false); // want to know if this ever happens...
				}
			} else {
			  Sleep(1); // doesn't seem to hurt cpu--"sleep x ms"
			  continue;
			}
        } else {
		  gotAnyAtAll = TRUE;
		  totalSuccessFullyread++;
		}
		
        // get the captured data
        BYTE *pData;
        UINT32 nNumFramesToRead;
        DWORD dwFlags;

		// I guess it gives us...as much audio as possible to read...probably

        hr = pAudioCaptureClient->GetBuffer(
            &pData,
            &nNumFramesToRead,
            &dwFlags,
            NULL,
            NULL
        ); // ACTUALLY GET THE BUFFER which I assume it reads in the format of the fella we passed in
        
        
        if (FAILED(hr)) {
            ShowOutput("IAudioCaptureClient::GetBuffer failed after %u frames: hr = 0x%08x\n", pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;            
        }

		{
  		  CAutoLock cAutoLockShared(&m_cSharedState);

			if( dwFlags == 0 ) {
			  // the good case
			  // we'll let fillbuffer set bFirstPacket = false; since it uses it to know if the next packet should restart, etc.
			} else if (bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
				ShowOutput("Probably spurious glitch reported on first packet, or two discontinuity errors occurred before it read from the cached buffer\n");
				bFirstPacket = true; // won't hurt, even if it is a real first packet :)
				// LODO it should probably clear the buffers if it ever gets discontinuity
				// or "let" it clear the buffers then send the new data on
				// as we have any left-over data that will be assigned a wrong timestamp
				// but it won't be too far wrong, compared to what it would otherwise be with always
				// assigning it the current graph timestamp, like we used to...
			} else if (AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
				  ShowOutput("IAudioCaptureClient::discontinuity GetBuffer set flags to 0x%08x after %u frames\n", dwFlags, pnFrames);
				  // expected your CPU gets behind or what not. I guess.
				  /*pAudioClient->Stop();
				  AvRevertMmThreadCharacteristics(hTask);
				  pAudioCaptureClient->Release();		  
				  pAudioClient->Release();            
				  return E_UNEXPECTED;*/
				  bFirstPacket = true;
			} else if (AUDCLNT_BUFFERFLAGS_SILENT == dwFlags) {
     		  // ShowOutput("IAudioCaptureClient::silence (just) from GetBuffer after %u frames\n", pnFrames);
			  // expected if there's silence (i.e. nothing playing), since we now include the "silence generator" work-around...
			} else {
			  // probably silence + discontinuity
     		  ShowOutput("IAudioCaptureClient::unknown discontinuity GetBuffer set flags to 0x%08x after %u frames\n", dwFlags, pnFrames);
			  bFirstPacket = true; // probably is some type of discontinuity :P
			}

			if(bFirstPacket)
				totalBlips++;

			if (0 == nNumFramesToRead) {
				ShowOutput("death failure: IAudioCaptureClient::GetBuffer said to read 0 frames after %u frames\n", pnFrames);
				pAudioClient->Stop();
				AvRevertMmThreadCharacteristics(hTask);
				pAudioCaptureClient->Release();
				pAudioClient->Release();
				return E_UNEXPECTED;            
			}

			pnFrames += nNumFramesToRead; // increment total count...		

			// lBytesToWrite typically 1792 bytes...
			LONG lBytesToWrite = nNumFramesToRead * nBlockAlign; // nBlockAlign is "audio block size" or frame size, for one audio segment...
			{
			  CAutoLock cObjectLock(&csMyLock);  // Lock the critical section, releases scope after block is over...

			  if(pBufLocalCurrentEndLocation > expectedMaxBufferSize) { 
				// this happens during VLC pauses...
				// I have no idea what I'm doing here... this doesn't fix it, but helps a bit... TODO FINISH THIS
				// it seems like if you're just straight recording then you want this big...otherwise you want it like size 0 and non-threaded [pausing with graphedit, for example]... [?]
				// if you were recording skype, you'd want it non realtime...hmm...
				// it seems that if the cpu is loaded, we run into this if it's for the next packet...hmm...
				// so basically we don't accomodate realtime at all currently...hmmm...
	  			ShowOutput("overfilled buffer, cancelling/flushing."); //over flow overflow appears VLC just keeps reading though, when paused [?] but not graphedit...or does it?
				pBufLocalCurrentEndLocation = 0;
				totalOverflows++;
				bFirstPacket = true;
			  }

			  for(INT i = 0; i < lBytesToWrite && pBufLocalCurrentEndLocation < expectedMaxBufferSize; i++) {
				pBufLocal[pBufLocalCurrentEndLocation++] = pData[i];
			  }
			}
		}
        
        hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        if (FAILED(hr)) {
            ShowOutput("IAudioCaptureClient::ReleaseBuffer failed after %u frames: hr = 0x%08x\n", pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;            
        }
        
		return hr;
    } // while !got anything && should continue loop

	return E_UNEXPECTED;

}

// iSize is max size of the BYTE buffer...so maybe...we should just drop it if we have past that size? hmm...we're probably
HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* totalBytesWrote)
 {
	while(!shouldStop) { // allow this to exit, too, at shutdown.
       {
        CAutoLock cObjectLock(&csMyLock);  // Lock the critical section, releases scope after block is done...
		if(pBufLocalCurrentEndLocation > 0) {
		  // fails lodo is that ok? 
		  // assert(pBufLocalCurrentEndLocation <= expectedMaxBufferSize);
		  int totalToWrite = MIN(pBufLocalCurrentEndLocation, expectedMaxBufferSize);
		  ASSERT(totalToWrite <= iSize); // just in case...just in case almost...
		  memcpy(pBuf, pBufLocal, totalToWrite);
          *totalBytesWrote = totalToWrite;
		  pBufLocalCurrentEndLocation = 0;
          return S_OK;
		} // else fall through to sleep outside the lock...
	  }
	  // sleep outside the lock ...
	  // using sleep doesn't seem to hurt the cpu
	  // and it seems to not get many "discontinuity" messages currently...
      Sleep(1);
	}
	return E_FAIL; // we didn't fill anything...and are shutting down...
}

// clean up
void loopBackRelease() {
	// tell running collector thread to end...
	shouldStop = 1;
	WaitForSingleObject(m_hThread, INFINITE);
    CloseHandle(m_hThread);
    m_hThread = NULL;
	pAudioClient->Stop();
    AvRevertMmThreadCharacteristics(hTask);
    pAudioCaptureClient->Release();
    pAudioClient->Release();
    m_pMMDevice->Release();
	// thread is done, we are exiting...
	pBufLocalCurrentEndLocation = 0;
	outputStats();
}

void outputStats() {
	wchar_t output[250];
	wsprintf(output, L"total reads %d total blips %d total overflows %d", totalSuccessFullyread , totalBlips, totalOverflows);
	set_config_string_setting(L"last_output", output);
}
