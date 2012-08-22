#define SECOND_FRACTIONS_TO_GRAB 16

// dangerous macros!
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define MAX(a, b) ((a) > (b) ? (a) : (b))
extern bool bFirstPacket;
void ShowOutput(const char *str, ...);
HRESULT set_config_string_setting(LPCTSTR szValueName, wchar_t *szToThis );

int getHtzRate();
int getChannels();

HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* sizeWrote);

#define BITS_PER_BYTE 8