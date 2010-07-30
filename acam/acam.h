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


// This class is exported from the acam.dll
class ACAM_API Cacam {
public:
	Cacam(void);
	// TODO: add your methods here.
};

extern ACAM_API int nacam;

ACAM_API int fnacam(void);
