#include "stdafx.h"

#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "common.h"

int read_config_setting(LPCTSTR szValueName, int default);

HRESULT get_mic(IMMDevice **ppMMDevice) {

    HRESULT hr = S_OK;
	int micIndex = (int) read_config_setting(L"MicId",-1);
	if (micIndex == -1)
		micIndex = 1;

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

	if (micIndex == 1 || micIndex == 0)		//micIndex 0 should never happen but just in case
	{
		// get the default render endpoint
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, ppMMDevice);
		pMMDeviceEnumerator->Release();
		if (FAILED(hr)) {
			printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
			return hr;
		}

		return S_OK;
	}
	else
	{
		int realIndex = micIndex - 2;	//Offset it for actual endpoint index
		IMMDeviceCollection *pMMDeviceCollection;

		// get all the active render endpoints
		hr = pMMDeviceEnumerator->EnumAudioEndpoints(
			eCapture, DEVICE_STATE_ACTIVE, &pMMDeviceCollection
		);

		UINT count;
		hr = pMMDeviceCollection->GetCount(&count);
		if (FAILED(hr)) {
			pMMDeviceCollection->Release();
			printf("IMMDeviceCollection::GetCount failed: hr = 0x%08x\n", hr);
			return hr;
		}

		for (UINT i = 0; i < count; i++) {
			IMMDevice *pMMDevice;

			// get the "n"th device
			hr = pMMDeviceCollection->Item(i, &pMMDevice);
			if (FAILED(hr)) {
				pMMDeviceCollection->Release();
				printf("IMMDeviceCollection::Item failed: hr = 0x%08x\n", hr);
				return hr;
			}

			if (i == realIndex)
			{
				hr = pMMDeviceCollection->Item(i, ppMMDevice);
				if (FAILED(hr)) {
					printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
					return hr;
				}
			}
		}
		pMMDeviceEnumerator->Release();
		if (FAILED(hr)) {
			printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
			return hr;
		}
		pMMDeviceCollection->Release();
		return S_OK;
	}
}

// I guess this is the default umm...audio output device?
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