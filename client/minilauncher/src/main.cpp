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
    else if (name[name.length()-1] == L'/' || name[name.length()-1] == L'\\')
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
    if (value[value.length()-1] != L'/')
        value += L'/';
    value += fileName;

    return value;
}

bool startProcessAsync(wchar_t* commandline, const wstring& dstDir)
{
    STARTUPINFO lpStartupInfo;
    PROCESS_INFORMATION lpProcessInfo;
    memset(&lpStartupInfo, 0, sizeof(lpStartupInfo));
    memset(&lpProcessInfo, 0, sizeof(lpProcessInfo));
    return CreateProcess(0, commandline,
        NULL, NULL, NULL, NULL, NULL,
        dstDir.c_str(),
        &lpStartupInfo,
        &lpProcessInfo);
}

int launchFile()
{
    try
    {
        wstring dstDir = getDstDir();
        wchar_t buffer[MAX_PATH*2 +3];
        wsprintf(buffer, L"\"%s\" \"%s\"", getFullFileName(dstDir, QN_CLIENT_EXECUTABLE_NAME).c_str(), dstDir.c_str());
        if (startProcessAsync(buffer, dstDir))
            return 0;
        return 1;
    } catch(...)
    {
        return -1;
    }
}

wstring unquoteStr(std::wstring str)
{
    if (str.empty())
        return str;
    if (str[0] == L'"')
        str = str.substr(1, MAX_PATH);
    if (str.empty())
        return str;
    if (str[str.length()-1] == L'"')
        str = str.substr(0, str.length()-1);
    return str;
}

int _tmain(int argc, _TCHAR* argv[])
{
    launchFile();
	return 0;
}

