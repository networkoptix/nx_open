// dllmain.cpp : Defines the entry point for the DLL application.

#include "targetver.h"

#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Windows Header Files:
#include <windows.h>

#include <atlpath.h>
#include <strsafe.h>

// WiX Header Files:
#include <Msi.h>
#include <MsiQuery.h>
#include <wcautil.h>

#include <shobjidl.h>

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID /*lpReserved*/)
{
    switch(ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        WcaGlobalInitialize(hModule);
        break;

    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}

