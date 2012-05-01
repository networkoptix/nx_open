// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the EVEASSOCCA_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// EVEASSOCCA_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef EVEASSOCCA_EXPORTS
#define EVEASSOCCA_API __declspec(dllexport)
#else
#define EVEASSOCCA_API __declspec(dllimport)
#endif

// This class is exported from the CustomActions.dll
class EVEASSOCCA_API CCustomActions {
public:
    CCustomActions(void);
    // TODO: add your methods here.
};

extern EVEASSOCCA_API int nCustomActions;

EVEASSOCCA_API int fnCustomActions(void);
