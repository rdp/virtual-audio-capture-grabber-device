#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>


HRESULT LoopbackCapture(
    IMMDevice *pMMDevice,
    HMMIO hFile,
    bool bInt16,
    HANDLE hStartedEvent,
    HANDLE hStopEvent,
    PUINT32 pnFrames
) {
    HRESULT hr;

    // activate an IAudioClient
    IAudioClient *pAudioClient;
    hr = pMMDevice->Activate(
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
    WAVEFORMATEX *pwfx;
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

    MMCKINFO ckRIFF = {0};
    MMCKINFO ckData = {0};
	/*
    hr = WriteWaveHeader(hFile, pwfx, &ckRIFF, &ckData);
    if (FAILED(hr)) {
        // WriteWaveHeader does its own logging
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        return hr;
    }*/

    // create a periodic waitable timer
	
    HANDLE hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
    if (NULL == hWakeUp) {
        DWORD dwErr = GetLastError();
        printf("CreateWaitableTimer failed: last error = %u\n", dwErr);
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }

    UINT32 nBlockAlign = pwfx->nBlockAlign;
    *pnFrames = 0;
    
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
        //CloseHandle(hWakeUp);
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

    // set the waitable timer
	// it will wake up, grab available data from the device, then sleep.

    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    BOOL bOK = SetWaitableTimer(
        hWakeUp,
        &liFirstFire,
        lTimeBetweenFires,
        NULL, NULL, FALSE
    );
    if (!bOK) {
        DWORD dwErr = GetLastError();
        printf("SetWaitableTimer failed: last error = %u\n", dwErr);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }
    
    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        printf("IAudioClient::Start failed: hr = 0x%08x\n", hr);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return hr;
    }
    SetEvent(hStartedEvent);
    
    // loopback capture loop
    HANDLE waitArray[2] = { hStopEvent, hWakeUp };
    DWORD dwWaitResult;

    bool bDone = false;
    bool bFirstPacket = true;
    // loop forever until bDone is set by the keyboard
    for (UINT32 nPasses = 0; !bDone; nPasses++) {
		// wait for either the timer to tick, or the keyboard to be hit
        dwWaitResult = WaitForMultipleObjects(
            ARRAYSIZE(waitArray), waitArray,
            FALSE, INFINITE
        );

        if (WAIT_OBJECT_0 == dwWaitResult) {
			// keyboard was hit
            printf("Received stop event after %u passes and %u frames\n", nPasses, *pnFrames);
            bDone = true;
            continue; // exits loop
        }

        if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
            printf("Unexpected WaitForMultipleObjects return value %u on pass %u after %u frames\n", dwWaitResult, nPasses, *pnFrames);
            pAudioClient->Stop();
            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            CloseHandle(hWakeUp);
            pAudioClient->Release();
            return E_UNEXPECTED;
        }

        // got a "wake up" event - see if there's data
        UINT32 nNextPacketSize;
        hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x\n", nPasses, *pnFrames, hr);
            pAudioClient->Stop();
            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            CloseHandle(hWakeUp);
            pAudioClient->Release();            
            return hr;
        }

        if (0 == nNextPacketSize) {
            // no data yet
            continue;
        }

        // get the captured data
        BYTE *pData;
        UINT32 nNumFramesToRead;
        DWORD dwFlags;

        hr = pAudioCaptureClient->GetBuffer(
            &pData,
            &nNumFramesToRead,
            &dwFlags,
            NULL,
            NULL
        ); // ACTUALLY GET THE BUFFER which I assume it reads in the format of the fella we passed in
        // so...it reads nNumFrames and calls it good or what?
        
        
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nPasses, *pnFrames, hr);
            pAudioClient->Stop();
            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            CloseHandle(hWakeUp);
            pAudioClient->Release();            
            return hr;            
        }

        if (bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) {
            printf("Probably spurious glitch reported on first packet\n");
        } else if (0 != dwFlags) {
            printf("IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, nPasses, *pnFrames);
            pAudioClient->Stop();
            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            CloseHandle(hWakeUp);
            pAudioClient->Release();            
            return E_UNEXPECTED;
        }

        if (0 == nNumFramesToRead) {
            printf("IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", nPasses, *pnFrames);
            pAudioClient->Stop();
            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            CloseHandle(hWakeUp);
            pAudioClient->Release();            
            return E_UNEXPECTED;            
        }

        LONG lBytesToWrite = nNumFramesToRead * nBlockAlign;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
        LONG lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(pData), lBytesToWrite);
        if (lBytesToWrite != lBytesWritten) {
            printf("mmioWrite wrote %u bytes on pass %u after %u frames: expected %u bytes\n", lBytesWritten, nPasses, *pnFrames, lBytesToWrite);
            pAudioClient->Stop();
            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            CloseHandle(hWakeUp);
            pAudioClient->Release();            
            return E_UNEXPECTED;
        }
        *pnFrames += nNumFramesToRead;
        
        hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        if (FAILED(hr)) {
            printf("IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x\n", nPasses, *pnFrames, hr);
            pAudioClient->Stop();
//            CancelWaitableTimer(hWakeUp);
            AvRevertMmThreadCharacteristics(hTask);
            pAudioCaptureClient->Release();
            //CloseHandle(hWakeUp);
            pAudioClient->Release();            
            return hr;            
        }
        
        bFirstPacket = false;
    } // capture loop
	// some pretty sick redundancy in here...

/*    hr = FinishWaveFile(hFile, &ckData, &ckRIFF);
    if (FAILED(hr)) {
        // FinishWaveFile does it's own logging
        pAudioClient->Stop();
        CancelWaitableTimer(hWakeUp);
        AvRevertMmThreadCharacteristics(hTask);
        pAudioCaptureClient->Release();
        CloseHandle(hWakeUp);
        pAudioClient->Release();
        return hr;
    }
	*/
    
    pAudioClient->Stop();
    //CancelWaitableTimer(hWakeUp);
    AvRevertMmThreadCharacteristics(hTask);
    pAudioCaptureClient->Release();
    //CloseHandle(hWakeUp);
    pAudioClient->Release();

    return hr;
}
