#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <tchar.h>
#include <Windows.h>
#include "version.h"
#include <iostream>
#include "Shlwapi.h"
#include <algorithm>

typedef signed __int64 int64_t;

using namespace std;

wstring closeDirPath(const wstring& name)
{
    if (name.empty())
        return name;
    else if (name[name.length() - 1] == L'/' || name[name.length() - 1] == L'\\')
        return name;
    else
        return name + L'/';
}

wstring getDstDir()
{
    wchar_t ownPath[MAX_PATH];

    // Will contain exe path
    HMODULE hModule = GetModuleHandle(NULL);
    if (hModule != NULL)
    {
        // When passing NULL to GetModuleHandle, it returns handle of exe itself
        GetModuleFileNameW(hModule, ownPath, MAX_PATH);
        PathRemoveFileSpecW(ownPath);
    }

    return wstring(closeDirPath(ownPath));
}

wstring getFullFileName(const wstring& folder, const wstring& fileName)
{
    if (folder.empty())
        return fileName;

    wstring value = folder;
    for (int i = 0; i < value.length(); ++i)
    {
        if (value[i] == L'\\')
            value[i] = L'/';
    }
    if (value[value.length() - 1] != L'/')
        value += L'/';
    value += fileName;

    return value;
}

BOOL startProcessAsync(wchar_t* commandline, const wstring& dstDir)
{
    STARTUPINFO lpStartupInfo;
    PROCESS_INFORMATION lpProcessInfo;
    memset(&lpStartupInfo, 0, sizeof(lpStartupInfo));
    memset(&lpProcessInfo, 0, sizeof(lpProcessInfo));

    return CreateProcess(
        0,                  /*<  _In_opt_   LPCTSTR               lpApplicationName */
        commandline,        /*< _Inout_opt_ LPTSTR                lpCommandLine */
        NULL,               /*< _In_opt_    LPSECURITY_ATTRIBUTES lpProcessAttributes */
        NULL,               /*< _In_opt_    LPSECURITY_ATTRIBUTES lpThreadAttributes */
        NULL,               /*< _In_        BOOL                  bInheritHandles */
        NULL,               /*< _In_        DWORD                 dwCreationFlags */
        NULL,               /*< _In_opt_    LPVOID                lpEnvironment */
        dstDir.c_str(),     /*< _In_opt_    LPCTSTR               lpCurrentDirectory */
        &lpStartupInfo,     /*< _In_        LPSTARTUPINFO         lpStartupInfo */
        &lpProcessInfo      /*< _Out_       LPPROCESS_INFORMATION lpProcessInformation */
    );
}

int launchFile()
{
    try
    {
        wstring dstDir = getDstDir();
        wchar_t buffer[MAX_PATH * 2 + 3];
        wsprintf(buffer, L"\"%s\" --exec \"%s\"", getFullFileName(dstDir, QN_APPLAUNCHER_BINARY_NAME).c_str(), dstDir.c_str());
        if (startProcessAsync(buffer, dstDir))
            return 0;
        return 1;
    }
    catch (...)
    {
        return -1;
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    (int)(argc);    /* Q_UNUSED */
    (void*)(argv);  /* Q_UNUSED */

    return launchFile();
}

