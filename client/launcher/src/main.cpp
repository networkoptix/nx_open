#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <tchar.h>
#include <Windows.h>
#include <Msi.h>
#include "version.h"
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
# ifndef BOOST_PREPROCESSOR_CAT_HPP
# define BOOST_PREPROCESSOR_CAT_HPP
#
//# include <boost/preprocessor/config/config.hpp>
#
# /* BOOST_PP_CAT */
#
# if ~BOOST_PP_CONFIG_FLAGS() & BOOST_PP_CONFIG_MWCC()
#    define BOOST_PP_CAT(a, b) BOOST_PP_CAT_I(a, b)
# else
#    define BOOST_PP_CAT(a, b) BOOST_PP_CAT_OO((a, b))
#    define BOOST_PP_CAT_OO(par) BOOST_PP_CAT_I ## par
# endif
#
# if ~BOOST_PP_CONFIG_FLAGS() & BOOST_PP_CONFIG_MSVC()
#    define BOOST_PP_CAT_I(a, b) a ## b
# else
#    define BOOST_PP_CAT_I(a, b) BOOST_PP_CAT_II(a ## b)
#    define BOOST_PP_CAT_II(res) res
# endif
#
# endif






#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef signed __int64 int64_t;
#else
typedef signed long long int64_t;
#endif

#define CLIENT_FILE_NAME BOOST_PP_CAT(L, QN_CLIENT_EXECUTABLE_NAME)

using namespace std;

static const int IO_BUFFER_SIZE = 1024*1024;
static const int64_t MAGIC = 0x73a0b934820d4055ll;

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
    int pos = fileName.find_last_of(L'/');
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

int createDirectory(const wstring& path) {
    wchar_t* buffer = new wchar_t[path.length() + 16];
    wsprintf(buffer, L"mkdir \"%s\"", toNativeSeparator(path).c_str());
    int result = _wsystem(buffer);
    delete [] buffer;
    return result;
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

int extractFile(ifstream& srcFile, const wstring& fullFileName, int64_t pos, int64_t fileSize)
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
    } while (srcFile.gcount() > 0 && bytesLeft > 0);

    dstFile.close();
    delete [] buffer;
    return 0;
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

int launchFile(const wstring& executePath)
{
    ifstream srcFile;
    srcFile.open(executePath.c_str(), std::ios_base::binary);
    if (!srcFile.is_open())
        return -1;

    srcFile.seekg(-sizeof(int64_t)*2, std::ios::end); // skip magic, and nov pos
    int64_t magic, novPos, indexTablePos;
    
    srcFile.read((char*) &novPos, sizeof(int64_t));
    srcFile.read((char*) &magic, sizeof(int64_t));
    if (magic != MAGIC)
        return -5;

    srcFile.seekg(novPos-sizeof(int64_t)); // go to index_pos field
    int64_t indexEofPos = (int64_t)srcFile.tellg(); // + sizeof(int64_t);
    srcFile.read((char*) &indexTablePos, sizeof(int64_t));

    if (indexEofPos - indexTablePos > 1024*16)
        return -3; // too long file catalog

    vector<int64_t> filePosList;
    vector<wstring> fileNameList;

    char* buffer = new char[indexEofPos - indexTablePos];
    try 
    {
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
        int i = 0;

        set<wstring> checkedDirs;
        checkDir(checkedDirs, dstDir);

        for (; i < filePosList.size()-1; ++i) {
            wstring fullFileName = getFullFileName(dstDir, fileNameList[i]);
            checkDir(checkedDirs, extractFilePath(fullFileName));
            extractFile(srcFile, fullFileName, filePosList[i], filePosList[i+1] - filePosList[i]);
        }
        srcFile.close();

        // check if MSVC MSI exists
        INSTALLSTATE state;
        if (sizeof(char*) == 4)
            state = MsiQueryProductState(L"{BD95A8CD-1D9F-35AD-981A-3E7925026EBB}");
        else
            state = MsiQueryProductState(L"{CF2BEA3C-26EA-32F8-AA9B-331F7E34BA97}");
        if (state != INSTALLSTATE_DEFAULT)
        {
            wchar_t buffer[MAX_PATH + 16];
            wchar_t* arch = sizeof(char*) == 4 ? L"x86" : L"x64";
            wsprintf(buffer, L"\"%s\\vcredist_%s.exe\" /q", toNativeSeparator(dstDir).c_str(), arch);
            int result = _wsystem(buffer);
        }

        // start client

        wchar_t buffer[MAX_PATH*2 +3];
        wsprintf(buffer, L"\"%s\" \"%s\"", getFullFileName(dstDir, CLIENT_FILE_NAME).c_str(), executePath.c_str());
        if (!startProcessAsync(buffer, dstDir))
        {
            // todo: refactor it. Current version have different 'exe' name for installer and debug mode. So, try both names
            wsprintf(buffer, L"\"%s\" \"%s\"", getFullFileName(dstDir, L"client.exe").c_str(), executePath.c_str());
            startProcessAsync(buffer, dstDir);
        }

        return 0;
    } catch(...)
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
    if (std::wstring(lpCmdLine).empty()) {
        wchar_t exepath[MAX_PATH];
        GetModuleFileName(0, exepath, MAX_PATH);
        launchFile(exepath);
    }
    else {
        launchFile(unquoteStr(lpCmdLine));
    }
    return 0;
}

/*
int _tmain(int argc, _TCHAR* argv[])
{
    if (argc == 1)
    {
        launchFile(wstring(argv[0]));
    }
    else if (argc == 2)
    {
        launchFile(wstring(argv[1]));
    }

	return 0;
}
*/
