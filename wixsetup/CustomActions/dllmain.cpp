// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
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

