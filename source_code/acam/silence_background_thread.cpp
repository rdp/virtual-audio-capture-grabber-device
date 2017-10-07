// silence.cpp from msdn blog
#include "stdafx.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>

#include "silence.h"

DWORD WINAPI PlaySilenceThreadFunction(LPVOID pContext) {
    PlaySilenceThreadFunctionArguments *pArgs =
        (PlaySilenceThreadFunctionArguments*)pContext;

    IMMDevice *pMMDevice = pArgs->pMMDevice;
    HRESULT &hr = pArgs->hr;

    // CoInitialize
    hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        printf("CoInitialize failed: hr = 0x%08x\n", hr);
        return 0;
    }

    // activate an IAudioClient
    IAudioClient *pAudioClient;
    hr = pMMDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL, NULL,
        (void**)&pAudioClient
    );
    if (FAILED(hr)) {
        printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        CoUninitialize();
        return 0;
    }

    // get the mix format
    WAVEFORMATEX *pwfx;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        printf("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // initialize the audio client
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, pwfx, NULL
    );
    CoTaskMemFree(pwfx);
    if (FAILED(hr)) {
        printf("IAudioClient::Initialize failed: hr 0x%08x\n", hr);
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // get the buffer size
    UINT32 nFramesInBuffer;
    hr = pAudioClient->GetBufferSize(&nFramesInBuffer);
    if (FAILED(hr)) {
        printf("IAudioClient::GetBufferSize failed: hr 0x%08x\n", hr);
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // get an IAudioRenderClient
    IAudioRenderClient *pAudioRenderClient;
    hr = pAudioClient->GetService(
        __uuidof(IAudioRenderClient),
        (void**)&pAudioRenderClient
    );
    if (FAILED(hr)) {
        printf("IAudioClient::GetService(IAudioRenderClient) failed: hr 0x%08x\n", hr);
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // create a "feed me" event
    HANDLE hFeedMe;
    hFeedMe = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hFeedMe) {
        DWORD dwErr = GetLastError();
        hr = HRESULT_FROM_WIN32(dwErr);
        printf("CreateEvent failed: last error is %u\n", dwErr);
        pAudioRenderClient->Release();
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // set it as the event handle
    hr = pAudioClient->SetEventHandle(hFeedMe);
    if (FAILED(hr)) {
        printf("IAudioClient::SetEventHandle failed: hr = 0x%08x\n", hr);
        pAudioRenderClient->Release();
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // pre-fill a single buffer of silence
    BYTE *pData;
    hr = pAudioRenderClient->GetBuffer(nFramesInBuffer, &pData);
    if (FAILED(hr)) {
        printf("IAudioRenderClient::GetBuffer failed on pre-fill: hr = 0x%08x\n", hr);
        pAudioClient->Stop();
        printf("TODO: unregister with MMCSS\n");
        CloseHandle(hFeedMe);
        pAudioRenderClient->Release();
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }

    // release the buffer with the silence flag
    hr = pAudioRenderClient->ReleaseBuffer(nFramesInBuffer, AUDCLNT_BUFFERFLAGS_SILENT);
    if (FAILED(hr)) {
        printf("IAudioRenderClient::ReleaseBuffer failed on pre-fill: hr = 0x%08x\n", hr);
        pAudioClient->Stop();
        printf("TODO: unregister with MMCSS\n");
        CloseHandle(hFeedMe);
        pAudioRenderClient->Release();
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }        

    // register with MMCSS
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(L"Playback", &nTaskIndex);
    if (NULL == hTask) {
        DWORD dwErr = GetLastError();
        hr = HRESULT_FROM_WIN32(dwErr);
        printf("AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
        pAudioRenderClient->Release();
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }    

    // call Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        printf("IAudioClient::Start failed: hr = 0x%08x\n", hr);
        printf("TODO: unregister with MMCSS\n");
        AvRevertMmThreadCharacteristics(hTask);
        pAudioRenderClient->Release();
        pAudioClient->Release();
        CoUninitialize();
        return 0;
    }
    SetEvent(pArgs->hStartedEvent);

    HANDLE waitArray[2] = { pArgs->hStopEvent, hFeedMe };
    DWORD dwWaitResult;

    bool bDone = false;
    for (UINT32 nPasses = 0; !bDone; nPasses++) {
        dwWaitResult = WaitForMultipleObjects(
            ARRAYSIZE(waitArray), waitArray,
            FALSE, INFINITE
        );

        if (WAIT_OBJECT_0 == dwWaitResult) {
            printf("Received stop event after %u passes\n", nPasses);
            bDone = true;
            continue; // exits loop
        }

        if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
            hr = E_UNEXPECTED;
            printf("Unexpected WaitForMultipleObjects return value %u on pass %u\n", dwWaitResult, nPasses);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            CloseHandle(hFeedMe);
            pAudioRenderClient->Release();
            pAudioClient->Release();
            CoUninitialize();
            return 0;
        }

        // got "feed me" event - see how much padding there is
        //
        // padding is how much of the buffer is currently in use
        //
        // note in particular that event-driven (pull-mode) render should not
        // call GetCurrentPadding multiple times
        // in a single processing pass
        // this is in stark contrast to timer-driven (push-mode) render
        UINT32 nFramesOfPadding;
        hr = pAudioClient->GetCurrentPadding(&nFramesOfPadding);
        if (FAILED(hr)) {
            ShowOutput("IAudioClient::GetCurrentPadding failed on pass %u: hr = 0x%08x\n", nPasses, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            CloseHandle(hFeedMe);
            pAudioRenderClient->Release();
            pAudioClient->Release();
            CoUninitialize();
            return 0;
        }

        if (nFramesOfPadding == nFramesInBuffer) {
            hr = E_UNEXPECTED;
            ShowOutput("Got \"feed me\" event but IAudioClient::GetCurrentPadding reports buffer is full - glitch?\n");
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            CloseHandle(hFeedMe);
            pAudioRenderClient->Release();
            pAudioClient->Release();
            CoUninitialize();
            return 0;
        }
    
        hr = pAudioRenderClient->GetBuffer(nFramesInBuffer - nFramesOfPadding, &pData);
        if (FAILED(hr)) {
            printf("IAudioRenderClient::GetBuffer failed on pass %u: hr = 0x%08x - glitch?\n", nPasses, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            CloseHandle(hFeedMe);
            pAudioRenderClient->Release();
            pAudioClient->Release();
            CoUninitialize();
            return 0;
        }

        // *** AT THIS POINT ***
        // If you wanted to render something besides silence,
        // you would fill the buffer pData
        // with (nFramesInBuffer - nFramesOfPadding) worth of audio data
        // this should be in the same wave format
        // that the stream was initialized with
        //
        // In particular, if you didn't want to use the mix format,
        // you would need to either ask for a different format in IAudioClient::Initialize
        // or do a format conversion
        //
        // If you do, then change the AUDCLNT_BUFFERFLAGS_SILENT flags value below to 0

        // release the buffer with the silence flag
        hr = pAudioRenderClient->ReleaseBuffer(nFramesInBuffer - nFramesOfPadding, AUDCLNT_BUFFERFLAGS_SILENT);
        if (FAILED(hr)) {
            printf("IAudioRenderClient::ReleaseBuffer failed on pass %u: hr = 0x%08x - glitch?\n", nPasses, hr);
            pAudioClient->Stop();
            AvRevertMmThreadCharacteristics(hTask);
            CloseHandle(hFeedMe);
            pAudioRenderClient->Release();
            pAudioClient->Release();
            CoUninitialize();
            return 0;
        }        
    } // for each pass

    pAudioClient->Stop();
    AvRevertMmThreadCharacteristics(hTask);
    CloseHandle(hFeedMe);
    pAudioRenderClient->Release();
    pAudioClient->Release();
    CoUninitialize();
    return 0;
}
