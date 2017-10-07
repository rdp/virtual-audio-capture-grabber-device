#include "common.h"
struct PlaySilenceThreadFunctionArguments {
    IMMDevice *pMMDevice;
    HANDLE hStartedEvent;
    HANDLE hStopEvent;
    HRESULT hr;
};