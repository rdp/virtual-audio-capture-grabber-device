#include "stdafx.h"
#include <stdio.h>
#include "silence.h"

IMMDevice *m_pMMSilenceDevice;

HANDLE hStartedEvent;
HANDLE hStopEvent;
HANDLE hThread;
DWORD WINAPI PlaySilenceThreadFunction(LPVOID pContext);

int start_silence_thread() {
    // create a "silence has started playing" event
    hStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hStartedEvent) {
        printf("CreateEvent failed: last error is %u\n", GetLastError());
        return __LINE__;
    }

    // create a "stop playing silence now" event
    hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hStopEvent) {
        printf("CreateEvent failed: last error is %u\n", GetLastError());
        CloseHandle(hStartedEvent);
        return __LINE__;
    }
    HRESULT hr = get_default_device(&m_pMMSilenceDevice);
	 if (FAILED(hr)) {
        printf("get_def_device failed: hr = 0x%08x\n", hr);
        return __LINE__;
    }
    // create arguments for silence-playing thread
    PlaySilenceThreadFunctionArguments threadArgs;
    threadArgs.hr = E_UNEXPECTED; // thread will overwrite this
    threadArgs.hStartedEvent = hStartedEvent;
    threadArgs.hStopEvent = hStopEvent;
    threadArgs.pMMDevice = m_pMMSilenceDevice;

    hThread = CreateThread(
        NULL, 0,
        PlaySilenceThreadFunction, &threadArgs,
        0, NULL
    );
    if (NULL == hThread) {
        printf("CreateThread failed: last error is %u\n", GetLastError());
        CloseHandle(hStopEvent);
        CloseHandle(hStartedEvent);
        return __LINE__;
    }

    // wait for either silence to start or the thread to end/abort unexpectedly :|
    HANDLE waitArray[2] = { hStartedEvent, hThread };
    DWORD dwWaitResult;
    dwWaitResult = WaitForMultipleObjects(
        ARRAYSIZE(waitArray), waitArray,
        FALSE, INFINITE
    );

    if (WAIT_OBJECT_0 + 1 == dwWaitResult) {
        printf("Thread aborted before starting to play silence: hr = 0x%08x\n", threadArgs.hr);
        CloseHandle(hStartedEvent);
        CloseHandle(hThread);
        CloseHandle(hStopEvent);
        return __LINE__;
    }

    if (WAIT_OBJECT_0 != dwWaitResult) {
        printf("Unexpected WaitForMultipleObjects return value %u: last error is %u\n", dwWaitResult, GetLastError());
        CloseHandle(hStartedEvent);
        CloseHandle(hThread);
        CloseHandle(hStopEvent);
        return __LINE__;
    }

    CloseHandle(hStartedEvent);
}

HRESULT join_silence_thread() {
	SetEvent(hStopEvent);
	WaitForSingleObject(hThread, INFINITE);
	// wait for the thread to terminate
	HANDLE rhHandles[1] = { hThread };

	DWORD exitCode;
	if (!GetExitCodeThread(hThread, &exitCode)) {
		printf("GetExitCodeThread failed: last error is %u\n", GetLastError());
		CloseHandle(hThread);
		CloseHandle(hStopEvent);
		return __LINE__;
	}

	if (0 != exitCode) {
		printf("Silence thread exit code is %u; expected 0\n", exitCode);
		CloseHandle(hThread);
		CloseHandle(hStopEvent);
		return __LINE__;
	}

	/*    if (S_OK != threadArgs.hr) { // didn't care enuf :|
	printf("Thread HRESULT is 0x%08x\n", threadArgs.hr);
	CloseHandle(hThread);
	CloseHandle(hStopEvent);
	return __LINE__;
	} */

	CloseHandle(hThread);
	CloseHandle(hStopEvent);
	return S_OK;
}
