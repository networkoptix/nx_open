// PropsCA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <objbase.h>
#include <shobjidl.h>
#include <atlstr.h>
#include <shlobj.h>

UINT __stdcall SetMoviesFolder(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "SetMoviesFolder");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    wchar_t moviesFolderPath[MAX_PATH];

    SHGetSpecialFolderPath(0, moviesFolderPath, CSIDL_MYVIDEO, TRUE);

    MsiSetProperty(hInstall, L"MoviesFolder", moviesFolderPath);

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
