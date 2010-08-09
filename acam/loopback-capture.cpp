#include "stdafx.h"

#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>

HRESULT open_file(LPCWSTR szFileName, HMMIO *phFile);

HRESULT get_default_device(IMMDevice **ppMMDevice);

// size is size of the BYTE buffer...but...I guess...we just have to fill it all the way with data...I guess...
HRESULT LoopbackCapture(const WAVEFORMATEX& wfex, BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly)
 {
	bool bInt16 = true; // makes it actually work, for some reason...

	UINT32 pnFrames = 0;

    HRESULT hr;
    IMMDevice *m_pMMDevice;
    hr = get_default_device(&m_pMMDevice); // so it can re-place our pointer...
    if (FAILED(hr)) {
        return hr;
    }

    // activate an (the default, for us) IAudioClient
    IAudioClient *pAudioClient;
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
        m_pMMDevice->Release();
		return hr;
	}

    MMCKINFO ckRIFF = {0};
    MMCKINFO ckData = {0};

    // create a periodic waitable timer
	
    UINT32 nBlockAlign = pwfx->nBlockAlign;
    
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
    IAudioCaptureClient *pAudioCaptureClient;
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pAudioCaptureClient
    );
    if (FAILED(hr)) {
        printf("IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
        //CloseHandle(hWakeUp);
        pAudioClient->Release();
        return hr;
    }
    
    // register with MMCSS
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
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
    
    bool bDone = false;
    bool bFirstPacket = true;


    // loop forever until bDone is set by the keyboard
    for (UINT32 nBitsWrote = 0; nBitsWrote < iSize; ) {

        // TODO sleep until there is data available [?] or can it poll me... [lodo]
        UINT32 nNextPacketSize;
        hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x\n", nBitsWrote, pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;
        }

        if (0 == nNextPacketSize) {
            // no data yet
			Sleep(0);// LODO (?)
            continue;
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
            printf("IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nBitsWrote, pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;            
        }

        if (bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
            printf("Probably spurious glitch reported on first packet\n");
        } else if (0 != dwFlags) {
            printf("IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, nBitsWrote, pnFrames);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return E_UNEXPECTED;
        }

        if (0 == nNumFramesToRead) {
            printf("IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", nBitsWrote, pnFrames);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return E_UNEXPECTED;            
        } else {
			pnFrames += nNumFramesToRead; // increment total count...
		}

        LONG lBytesToWrite = nNumFramesToRead * nBlockAlign;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
        // TODO WRITE TO OUTGOING [?]
		for(int i = 0; i < lBytesToWrite && nBitsWrote < iSize;i++) {

			pBuf[nBitsWrote++] = pData[i]; // lodo use a straight call...

		}
        
        hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nBitsWrote, pnFrames, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            pAudioClient->Release();            
            return hr;            
        }
        
        bFirstPacket = false;
    } // capture loop...

    pAudioClient->Stop();
    AvRevertMmThreadCharacteristics(hTask);
    pAudioCaptureClient->Release();
    pAudioClient->Release();
    m_pMMDevice->Release();
    return hr;
}

