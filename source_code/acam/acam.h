// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the ACAM_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// ACAM_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef ACAM_EXPORTS
#define ACAM_API __declspec(dllexport)
#else
#define ACAM_API __declspec(dllimport)
#endif


#include <streams.h>
#include <initguid.h>
#include <dllsetup.h>
#include <stdio.h>
#include <Windows.h>
#include "common.h"
#include <assert.h>

HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* sizeWrote);

#define BITS_PER_BYTE 8

extern CCritSec m_cSharedState;

// This class is exported from the acam.dll
class ACAM_API Cacam {
public:
	Cacam(void);
	// TODO: add your methods here.
};

extern ACAM_API int nacam;

ACAM_API int fnacam(void);

EXTERN_C const GUID CLSID_VirtualCam; // reuse it...


class CVCam : public CSource // not needed -> public IMediaFilter
{
public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFilterGraph *GetGraph() {return m_pGraph;};

    //////////////////////////////////////////////////////////////////////////
    //  IMediaFilter [added]
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Run(REFERENCE_TIME tStart);
	STDMETHODIMP GetState(DWORD dw, FILTER_STATE *pState);

	//protected:

   // IReferenceClock *m_pClock; // wrong place I think


private:
    CVCam(LPUNKNOWN lpunk, HRESULT *phr);
};

class CVCamStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet//, public IAMPushSource
{
public:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }                                                          \
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

	// IAMPushSource [not implemented all the way yet]
	/*HRESULT STDMETHODCALLTYPE GetLatency(REFERENCE_TIME *);
	HRESULT STDMETHODCALLTYPE GetPushSourceFlags(ULONG *);
	HRESULT STDMETHODCALLTYPE SetPushSourceFlags(ULONG);
	HRESULT STDMETHODCALLTYPE SetStreamOffset(REFERENCE_TIME) { return E_FAIL; }
	HRESULT STDMETHODCALLTYPE GetStreamOffset(REFERENCE_TIME *);
	HRESULT STDMETHODCALLTYPE GetMaxStreamOffset(REFERENCE_TIME *);
	HRESULT STDMETHODCALLTYPE SetMaxStreamOffset(REFERENCE_TIME);*/

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);
    
    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream
    //////////////////////////////////////////////////////////////////////////
    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);
    HRESULT OnThreadDestroy(void);
	/* don't seem to get called... */
    HRESULT Stop(void);
    HRESULT Exit(void);
    HRESULT Inactive(void); // ondisconnect :)

    CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName);
    ~CVCamStream();
    CRefTime     m_rtSampleEndTime;    // The time to be stamped on each sample

private:
    CVCam *m_pParent;
    // unused now? REFERENCE_TIME m_rtLastTime;
    HBITMAP m_hLogoBmp;
    IReferenceClock *m_pClock;

    LONGLONG m_llSampleMediaTimeStart;

	HRESULT setAsNormal(CMediaType *pmt);

};
