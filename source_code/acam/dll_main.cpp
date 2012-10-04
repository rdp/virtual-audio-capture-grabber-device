// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <olectl.h>

//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////


#include <streams.h>
#include <initguid.h>
#include <dllsetup.h>
#include <stdio.h>



#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")

#ifdef _DEBUG
    #pragma comment(lib, "strmbasd")
#else
    #pragma comment(lib, "strmbase")
#endif

#include <olectl.h>
#include <initguid.h>
#include <dllsetup.h>
#include "acam.h"

#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer( CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType     = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer( CLSID clsServer );

#ifdef _WIN64
// was {8E14549A-DB61-4309-AFA1-3578E927E933}
// {8E14549B-DB61-4309-AFA1-3578E927E935} now...
DEFINE_GUID(CLSID_VirtualCam,
            0x0e14549d, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x39);
#else
// was {8E14549A-DB61-4309-AFA1-3578E927E933}
// {8E14549B-DB61-4309-AFA1-3578E927E935} now...
DEFINE_GUID(CLSID_VirtualCam,
            0x0e14549d, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x39);
#endif

const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam = 
{ &MEDIATYPE_Audio      // clsMajorType
, &MEDIASUBTYPE_NULL }; // clsMinorType

const AMOVIESETUP_PIN AMSPinVCam=
{
    L"Output",             // Pin string name
    FALSE,                 // Is it rendered
    TRUE,                  // Is it an output
    FALSE,                 // Can we have none
    FALSE,                 // Can we have many
    &CLSID_NULL,           // Connects to filter
    NULL,                  // Connects to pin
    1,                     // Number of types
    &AMSMediaTypesVCam      // Pin Media types
};

const AMOVIESETUP_FILTER AMSFilterVCam =
{
    &CLSID_VirtualCam,  // Filter CLSID
    L"FFsplit Microphone Device",     // String name
    MERIT_DO_NOT_USE,      // Filter merit
    1,                     // Number pins
    &AMSPinVCam             // Pin details
};

CFactoryTemplate g_Templates[] = 
{
    {
        L"FFsplit Microphone Device",
        &CLSID_VirtualCam,
        CVCam::CreateInstance,
        NULL,
        &AMSFilterVCam
    },

};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

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
        hr = AMovieSetupRegisterServer(CLSID_VirtualCam, L"FFsplit Microphone Device", achFileName, L"Both", L"InprocServer32");
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
                rf2.rgPins = &AMSPinVCam;
                hr = fm->RegisterFilter(CLSID_VirtualCam, L"FFsplit Microphone Device", &pMoniker, &CLSID_AudioInputDeviceCategory, NULL, &rf2);
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_AudioInputDeviceCategory, 0, CLSID_VirtualCam);
            }
        }

      // release interface
      if(fm)
          fm->Release();
    }

    if( SUCCEEDED(hr) && !bRegister )
        hr = AMovieSetupUnregisterServer( CLSID_VirtualCam );

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// DLL.cpp

//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>

STDAPI RegisterFilters( BOOL bRegister );

STDAPI DllRegisterServer()
{
	//printf("hello there"); // we actually never see this...
    return RegisterFilters(TRUE);
}

STDAPI DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

STDAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

extern "C" BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

