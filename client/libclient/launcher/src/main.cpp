#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <tchar.h>
#include <Windows.h>
#include <Commctrl.h>
#include "version.h"
#include "progress.h"
#include "resource.h"
#include <iostream>

// -------------------------------------------------------------------------- //
// boost/preprocessor/cat.hpp
// -------------------------------------------------------------------------- //
# /* Copyright (C) 2001
#  * Housemarque Oy
#  * http://www.housemarque.com
#  *
#  * Distributed under the Boost Software License, Version 1.0. (See
#  * accompanying file LICENSE_1_0.txt or copy at
#  * http://www.boost.org/LICENSE_1_0.txt)
#  */
#
# /* Revised by Paul Mensonides (2002) */
#
# /* See http://www.boost.org for most recent version. */
#
#ifndef BOOST_PREPROCESSOR_CAT_HPP
#define BOOST_PREPROCESSOR_CAT_HPP
#
#define BOOST_PP_CAT(a, b) BOOST_PP_CAT_OO((a, b))
#define BOOST_PP_CAT_OO(par) BOOST_PP_CAT_I ## par
#
#define BOOST_PP_CAT_I(a, b) BOOST_PP_CAT_II(a ## b)
#define BOOST_PP_CAT_II(res) res
#
#endif

typedef signed __int64 int64_t;

#define CLIENT_FILE_NAME BOOST_PP_CAT(L, QN_CLIENT_EXECUTABLE_NAME)

using namespace std;

static const int IO_BUFFER_SIZE = 1024*1024;
static const int64_t MAGIC = 0x73a0b934820d4055ll;

wstring loadString(UINT id)
{
    enum { BUFFER_SIZE = 1024 };
    wchar_t buffer[BUFFER_SIZE];
    LoadString(0, IDS_UNPACKING, buffer, BUFFER_SIZE);
    return buffer;
}

wstring closeDirPath(const wstring& name)
{
    if (name.empty())
        return name;
    else if (name[name.length()-1] == L'/' || name[name.length()-1] == L'\\')
        return name;
    else
        return name + L'/';
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

wstring getDstDir()
{
    wchar_t path[MAX_PATH];

    GetTempPath(MAX_PATH, path);
    return wstring(closeDirPath(path) + L"nov_video");
    //return L"c:/client test/123";
}

wstring extractFilePath(const wstring& fileName)
{
    size_t pos = fileName.find_last_of(L'/');
    return fileName.substr(0, pos);
}

wchar_t getNativeSeparator()
{
    return L'\\';
}

wstring toNativeSeparator(const wstring& path)
{
    wstring value = path;
    for (int i = 0; i < value.length(); ++i)
    {
        if (value[i] == L'/' || value[i] == L'\\')
            value[i] = getNativeSeparator();
    }
    return value;
}

bool createDirectory(const wstring& path)
{
    return CreateDirectory(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

void checkDir(set<wstring>& checkedDirs, const wstring& dir)
{
    if (dir.empty())
        return;
    wstring normalizedName = toNativeSeparator(dir);
    if(normalizedName[normalizedName.size()-1] == getNativeSeparator())
        normalizedName = normalizedName.substr(0, normalizedName.length()-1);
    if (checkedDirs.count(normalizedName) > 0)
        return;
    checkedDirs.insert(normalizedName);

    if(GetFileAttributes(normalizedName.c_str()) == INVALID_FILE_ATTRIBUTES)
        createDirectory(normalizedName);
}

int extractFile(ifstream& srcFile, const wstring& fullFileName, int64_t pos, int64_t fileSize, QnLauncherProgress& progress)
{
    srcFile.seekg(pos);
    ofstream dstFile;
    dstFile.open(fullFileName.c_str(), std::ios_base::binary);
    if (!dstFile.is_open())
        return -1;
    char* buffer = new char[1024*1024];
    int64_t bytesLeft = fileSize;

    do {
        srcFile.read(buffer, min(bytesLeft, 1024*1024ll));
        dstFile.write(buffer, srcFile.gcount());
        bytesLeft -= srcFile.gcount();
        pos += srcFile.gcount();
        progress.setPos(pos);
    } while (srcFile.gcount() > 0 && bytesLeft > 0);

    dstFile.close();
    delete [] buffer;
    return 0;
}

BOOL startProcessAsync(wchar_t* commandline, const wstring& dstDir)
{
    STARTUPINFO lpStartupInfo;
    PROCESS_INFORMATION lpProcessInfo;
    memset(&lpStartupInfo, 0, sizeof(lpStartupInfo));
    memset(&lpProcessInfo, 0, sizeof(lpProcessInfo));

    return CreateProcess(
        0,              /*< lpApplicationName */
        commandline,    /*< lpCommandLine */
        NULL,           /*< lpProcessAttributes */
        NULL,           /*< lpThreadAttributes */
        false,          /*< bInheritHandles */
        NULL,           /*< dwCreationFlags */
        NULL,           /*< lpEnvironment */
        dstDir.c_str(), /*< lpCurrentDirectory */
        &lpStartupInfo, /*< lpStartupInfo */
        &lpProcessInfo  /*< lpProcessInformation */
    );
}

int launchFile(const wstring& executePath)
{
    static const int kMaximumFileCatalogSize = 1024 * 1024;

    ifstream srcFile;
    srcFile.open(executePath.c_str(), std::ios_base::binary);
    if (!srcFile.is_open())
        return -1;

    // see seekg(-value, std::ios::end) is broken in MSVC2012 for x86 mode. Workaround it
    srcFile.seekg(0, std::ios::end);
    int64_t pos = (int64_t) srcFile.tellg() - sizeof(int64_t) * 2; // skip magic, and nov pos
    srcFile.seekg(pos);

    int64_t magic = 0, novPos = 0, indexTablePos = 0;

    srcFile.read((char*) &novPos, sizeof(int64_t));
    srcFile.read((char*) &magic, sizeof(int64_t));
    if (magic != MAGIC)
        return -5;

    srcFile.seekg(novPos-sizeof(int64_t)); // go to index_pos field
    int64_t indexEofPos = (int64_t)srcFile.tellg(); // + sizeof(int64_t);
    srcFile.read((char*) &indexTablePos, sizeof(int64_t));

    auto catalogSize = indexEofPos - indexTablePos;
    if (catalogSize > kMaximumFileCatalogSize)
        return -3; // too long file catalog

    char* buffer = new char[catalogSize];
    try
    {
        vector<int64_t> filePosList;
        vector<wstring> fileNameList;
        srcFile.seekg(indexTablePos);
        int64_t curPos = indexTablePos;
        while (srcFile.tellg() < indexEofPos)
        {
            int64_t builtinFilePos;
            srcFile.read((char*)&builtinFilePos, sizeof(int64_t));
            if (builtinFilePos > indexEofPos)
                return -3; // invalid catalog
            filePosList.push_back(builtinFilePos);

            int fileNameSize;
            wchar_t builtinFileName[MAX_PATH+1];
            srcFile.read((char*)&fileNameSize, sizeof(int));
            if (fileNameSize > MAX_PATH)
                return -3; // invalid catalog
            srcFile.read((char*) &builtinFileName, fileNameSize*2);
            builtinFileName[fileNameSize] = L'\0';

            fileNameList.push_back(builtinFileName);
        }
        delete [] buffer;
        wstring dstDir = getDstDir();

        set<wstring> checkedDirs;
        checkDir(checkedDirs, dstDir);

        if (filePosList.size() > 1)
        {
            QnLauncherProgress progress(std::wstring(QN_CLIENT_DISPLAY_NAME)
                + L" - " + loadString(IDS_UNPACKING).c_str());

            progress.setRange(filePosList.front(), filePosList.back());

            for (int i = 0; i < filePosList.size() - 1; ++i)
            {
                wstring fullFileName = getFullFileName(dstDir, fileNameList[i]);
                checkDir(checkedDirs, extractFilePath(fullFileName));
                extractFile(srcFile, fullFileName, filePosList[i], filePosList[i+1] - filePosList[i], progress);
            }
        }

        srcFile.close();

        // start client
        static const wchar_t kPathTemplate[](L"\"%s\" \"%s\" --exported");
        wchar_t buffer[sizeof(kPathTemplate) / sizeof(kPathTemplate[0]) + (MAX_PATH-2)*2];
        wsprintf(buffer, kPathTemplate, getFullFileName(dstDir, CLIENT_FILE_NAME).c_str(), executePath.c_str());
        if (!startProcessAsync(buffer, dstDir))
        {
            // todo: refactor it. Current version have different 'exe' name for installer and debug mode. So, try both names
            wsprintf(buffer, kPathTemplate, getFullFileName(dstDir, L"desktop_client.exe").c_str(), executePath.c_str());
            startProcessAsync(buffer, dstDir);
        }

        return 0;
    }
    catch(...)
    {
        delete [] buffer;
        return -2;
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

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    INITCOMMONCONTROLSEX initCtrlEx;
    memset(&initCtrlEx, 0, sizeof(initCtrlEx));
    initCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrlEx.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&initCtrlEx);

    if (std::wstring(lpCmdLine).empty())
    {
        wchar_t exepath[MAX_PATH];
        GetModuleFileName(0, exepath, MAX_PATH);
        launchFile(exepath);
    }
    else
    {
        launchFile(unquoteStr(lpCmdLine));
    }
    return 0;
}
