#include "stdafx.h"
#include <stdio.h>

/* unused ...
void logToFile(char *log_this) {
    FILE *f;
	f = fopen("g:\\yo2", "a");
	fprintf(f, log_this);
	fclose(f);
} */

void ShowOutput(const char *str, ...)
{
#ifdef _DEBUG  // avoid in release mode…
  char buf[2048];
  va_list ptr;
  va_start(ptr,str);
  vsprintf_s(buf,str,ptr);
  OutputDebugStringA(buf);
  OutputDebugStringA("\n");
  //logToFile(buf);
#endif
}

HRESULT set_config_string_setting(LPCTSTR szValueName, wchar_t *szToThis ) {

   HKEY hKey = NULL;
   LONG i;

    DWORD dwDisp = 0;
    LPDWORD lpdwDisp = &dwDisp;

    i = RegCreateKeyEx(HKEY_CURRENT_USER,
       L"SOFTWARE\\virtual_audio_capture", 0L, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, lpdwDisp); // fails in flash player...

    if (i == ERROR_SUCCESS)
    {
		// bad for XP  ... = RegSetKeyValueA(hKey, NULL, szValueName, REG_SZ, ...
        i = RegSetValueEx(hKey, szValueName, 0, REG_SZ, (LPBYTE) szToThis, wcslen(szToThis)*2+1);

    } else {
       // failed to create key...
	}

	if(hKey)
	  RegCloseKey(hKey);
	return i;

}
