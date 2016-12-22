#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <iostream>

#include <tchar.h>
#include <Windows.h>
#include "Shlwapi.h"
#include "shlobj.h"

#include "version.h"

typedef signed __int64 int64_t;

std::wstring utf8toUtf16(const std::string& str)
{
    std::wstring_convert< std::codecvt_utf8_utf16<wchar_t> > converter;
    return converter.from_bytes(str);
}

std::wstring closeDirPath(const std::wstring& name)
{
    if (name.empty())
        return name;
    else if (name[name.length() - 1] == L'/' || name[name.length() - 1] == L'\\')
        return name;
    else
        return name + L'/';
}

std::wstring getSharedApplauncherDir()
{
    wchar_t result[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, result)))
    {
        // Append product-specific path
        PathAppend(result, L"\\" QN_ORGANIZATION_NAME  L"\\applauncher\\"  QN_CUSTOMIZATION_NAME);
    }
    return std::wstring(closeDirPath(result));

}

std::wstring getInstalledApplauncherDir()
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

    return std::wstring(closeDirPath(ownPath));
}

std::wstring getFullFileName(const std::wstring& folder, const std::wstring& fileName)
{
    if (folder.empty())
        return fileName;

    std::wstring value = folder;
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

BOOL startProcessAsync(wchar_t* commandline, const std::wstring& dstDir)
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

bool launchInDir(const std::wstring& dir, int argc, _TCHAR* argv[])
{
    std::wcout << L"Launching in path " << dir;
    try
    {
        wchar_t buffer[MAX_PATH * 2 + 3];
        int offset = wsprintf(buffer, L"\"%s\"", getFullFileName(dir, QN_APPLAUNCHER_BINARY_NAME).c_str(), dir.c_str());

        for (int i = 1; i < argc; ++i)
        {
            std::wstring argument(utf8toUtf16(argv[i]));
            offset += wsprintf(buffer + offset, L" %s", argument.c_str());
        }

        wsprintf(buffer + offset, L" \"%s\"", dir.c_str());
        if (startProcessAsync(buffer, dir))
            return true;
        return false;
    }
    catch (...)
    {
        return false;
    }
}

int launchFile(int argc, _TCHAR* argv[])
{
    if (launchInDir(getSharedApplauncherDir(), argc, argv))
        return 0;

    if (launchInDir(getInstalledApplauncherDir(), argc, argv))
        return 0;

    return 1;
}

int _tmain(int argc, _TCHAR* argv[])
{
    try
    {
        /* All additional arguments are passed to applauncher. */
        return launchFile(argc, argv);
    }
    catch (...)
    {
        return 2;
    }
}

