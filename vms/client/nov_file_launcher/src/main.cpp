// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <boost/preprocessor/cat.hpp>

#include <tchar.h>
#include <Windows.h>
#include <shellapi.h>
#include <Commctrl.h>
#include <Shlobj.h>
#include "version.h"
#include "progress.h"
#include "resource.h"

typedef signed __int64 int64_t;

#define CLIENT_FILE_NAME BOOST_PP_CAT(L, QN_CLIENT_EXECUTABLE_NAME)

using namespace std;

static const int IO_BUFFER_SIZE = 1024*1024;
static const int64_t MAGIC = 0x73a0b934820d4055ll;

wofstream logfile;

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

wstring getTempPath()
{
    wchar_t path[MAX_PATH];
    GetTempPath(MAX_PATH, path);
    return wstring(closeDirPath(path));
}

wstring getDstDir()
{
    return getTempPath() + L"nov_video";
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
    logfile << L"Create directory: " << path << L"\n";
    const auto result = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    const bool success = result == ERROR_SUCCESS
        || result == ERROR_ALREADY_EXISTS
        || result == ERROR_FILE_EXISTS;

    if (!success)
        logfile << L"Error: directory was not created: " << result << L"\n";

    return success;
}

void checkDir(set<wstring>& checkedDirs, const wstring& dir)
{
    logfile << L"Check Dir: " << dir.c_str() << L"\n";
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
        srcFile.read(buffer, std::min(bytesLeft, 1024*1024ll));
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
        nullptr, /*< lpApplicationName */
        commandline, /*< lpCommandLine */
        nullptr, /*< lpProcessAttributes */
        nullptr, /*< lpThreadAttributes */
        false, /*< bInheritHandles */
        0, /*< dwCreationFlags */
        nullptr, /*< lpEnvironment */
        dstDir.c_str(), /*< lpCurrentDirectory */
        &lpStartupInfo, /*< lpStartupInfo */
        &lpProcessInfo  /*< lpProcessInformation */
    );
}

/**
 * Unpack and launch client with nov file passed via command line.
 * Packed file structure:
 * /--------------------/
 * / launcher.exe       /
 * /--------------------/
 * / library1.dll       /
 * / library2.dll       /
 * / ...                /
 * / HD Witness.exe     /
 * / ...                /
 * / misc client files  /
 * /--------------------/
 * / INDEX table        /
 * / INDEX PTR: 8 bytes /
 * /--------------------/
 * / NOV file           /
 * / NOV PTR: 8 bytes   /
 * /--------------------/
 * / MAGIC: 8 bytes     /
 * /--------------------/
 */
int launchFile(const wstring& executePath)
{
    logfile << L"Launch file " << executePath << "\n";

    static const int kMaximumFileCatalogSize = 1024 * 1024;

    ifstream srcFile;
    srcFile.open(executePath.c_str(), std::ios_base::binary);
    if (!srcFile.is_open())
    {
        logfile << L"Cannot open file " << executePath << "\n";
        return -1;
    }

    srcFile.seekg(-sizeof(int64_t) * 2, std::ios::end);
    const int64_t novPosOffset = srcFile.tellg();

    int64_t magic = 0, novPos = 0, indexTablePos = 0;

    srcFile.read((char*) &novPos, sizeof(int64_t));
    srcFile.read((char*) &magic, sizeof(int64_t));
    if (magic != MAGIC)
    {
        logfile << L"Magic was not found" << "\n";
        return -5;
    }

    srcFile.seekg(novPos-sizeof(int64_t)); // go to index_pos field
    const int64_t indexEndOffset = novPos - sizeof(int64_t);

    srcFile.read((char*) &indexTablePos, sizeof(int64_t));

    auto catalogSize = indexEndOffset - indexTablePos;
    if (catalogSize > kMaximumFileCatalogSize)
    {
        logfile << L"Too long file catalog " << catalogSize << "\n";
        return -3;
    }

    try
    {
        vector<int64_t> filePosList;
        vector<wstring> fileNameList;
        srcFile.seekg(indexTablePos);
        int64_t curPos = indexTablePos;
        while (srcFile.tellg() < indexEndOffset)
        {
            int64_t builtinFilePos;
            srcFile.read((char*)&builtinFilePos, sizeof(int64_t));
            if (builtinFilePos > indexEndOffset)
            {
                logfile << L"Invalid catalog" << "\n";
                return -3;
            }
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
        wstring dstDir = getDstDir();

        logfile << L"Preparing folder: " << dstDir << "\n";

        set<wstring> checkedDirs;
        checkDir(checkedDirs, dstDir);

        if (!filePosList.empty())
        {
            QnLauncherProgress progress(std::wstring(QN_CLIENT_DISPLAY_NAME)
                + L" - " + loadString(IDS_UNPACKING).c_str());

            // Since we are calculating embedded file size as a difference between positions inside
            // the launcher file, we need to determine where the last embedded file ends.
            // We can use index table file position because it goes right after embedded files data.
            filePosList.push_back(indexTablePos);

            progress.setRange(filePosList.front(), filePosList.back());

            for (int i = 0; i < filePosList.size() - 1; ++i)
            {
                wstring fullFileName = getFullFileName(dstDir, fileNameList[i]);
                logfile << L"Export file: " << fullFileName.c_str() << L"\n";
                checkDir(checkedDirs, extractFilePath(fullFileName));
                extractFile(srcFile, fullFileName, filePosList[i], filePosList[i+1] - filePosList[i], progress);
            }
        }

        srcFile.close();

        // start client
        static const wchar_t kPathTemplate[](L"\"%s\" \"%s\" --exported");
        wchar_t buffer[sizeof(kPathTemplate) / sizeof(kPathTemplate[0]) + (MAX_PATH-2)*2];
        wsprintf(buffer, kPathTemplate, getFullFileName(dstDir, CLIENT_FILE_NAME).c_str(), executePath.c_str());
        logfile << L"Starting process.. " << buffer << "\n";
        startProcessAsync(buffer, dstDir);
        return 0;
    }
    catch(...)
    {
        logfile << "Exception caught";
        return -2;
    }
}

wstring trimAndUnquote(std::wstring str)
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

int main(int argc, char** argv)
{
    logfile.open(getTempPath() + L"nov_file_launcher.log", std::ofstream::out);

    INITCOMMONCONTROLSEX initCtrlEx;
    memset(&initCtrlEx, 0, sizeof(initCtrlEx));
    initCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrlEx.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&initCtrlEx);

    auto cmdLine = std::wstring(GetCommandLineW());
    logfile << cmdLine;

    auto szArglist = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!szArglist)
    {
        logfile << "Win api call CommandLineToArgvW failed";
    }
    else
    {
        std::wstring exePath(szArglist[argc - 1]);
        launchFile(exePath);
    }
    /*
    for( i=0; i<nArgs; i++) printf("%d: %ws\n", i, szArglist[i]);

    if (cmdLine.empty())
    {
        wchar_t exepath[MAX_PATH];
        GetModuleFileNameW(0, exepath, MAX_PATH);
        launchFile(exepath);
    }
    else
    {
        launchFile(trimAndUnquote(cmdLine));
    }*/

    LocalFree(szArglist);

    logfile.close();
    return 0;
}
