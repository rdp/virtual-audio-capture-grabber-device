//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#define AFX_DATA AFX_EXT_DATA
#include <stdio.h>

STDAPI RegisterFilters( BOOL bRegister );

extern "C" __declspec(dllexport) HRESULT DllRegisterServer()
{
	printf("hello there");
    return RegisterFilters(TRUE);
}

extern "C" __declspec(dllexport) HRESULT DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

STDAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

