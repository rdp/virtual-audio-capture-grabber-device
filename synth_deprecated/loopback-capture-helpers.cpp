
#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>
#include <stdio.h>

HRESULT open_file(LPCWSTR szFileName, HMMIO *phFile) {
    MMIOINFO mi = {0};

    *phFile = mmioOpen(
        // some flags cause mmioOpen write to this buffer
        // but not any that we're using
        const_cast<LPWSTR>(szFileName),
        &mi,
        MMIO_WRITE | MMIO_CREATE
    );

    if (NULL == *phFile) {
        printf("mmioOpen(\"%ls\", ...) failed. wErrorRet == %u\n", szFileName, mi.wErrorRet);
        return E_FAIL;
    }

    return S_OK;
}


HRESULT get_default_device(IMMDevice **ppMMDevice) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pMMDeviceEnumerator;
    // activate a device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
    );
    if (FAILED(hr)) {
        printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
        return hr;
    }

    // get the default render endpoint
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, ppMMDevice);
    pMMDeviceEnumerator->Release();
    if (FAILED(hr)) {
        printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
        return hr;
    }

    return S_OK;
}
