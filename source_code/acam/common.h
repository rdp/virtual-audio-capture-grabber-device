#include <mmdeviceapi.h>
#include <mmsystem.h>
#define SECOND_FRACTIONS_TO_GRAB 16

// dangerous macros!
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define MAX(a, b) ((a) > (b) ? (a) : (b))
extern bool bDiscontinuityDetected;
extern bool bVeryFirstPacket;
void ShowOutput(const char *str, ...);
HRESULT set_config_string_setting(LPCTSTR szValueName, wchar_t *szToThis );

int getHtzRate(HRESULT *hr);
int getBitsPerSample();
int getChannels();

HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* sizeWrote);

#define BITS_PER_BYTE 8

#define VIRTUAL_AUDIO_VERSION L"0.6.0"

void LoopbackCaptureClear();

HRESULT get_default_device(IMMDevice **ppMMDevice);