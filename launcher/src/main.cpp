#include <set>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <tchar.h>
#include "dirent.h"

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef signed __int64 int64_t;
#else
typedef signed long long int64_t;
#endif

using namespace std;

static const int IO_BUFFER_SIZE = 1024*1024;
static const int64_t MAGIC = 0x73a0b934820d4055ll;

int appendFile(ofstream& dstFile, const wstring& srcFileName)
{
    ifstream srcFile;
    char* buffer = new char[IO_BUFFER_SIZE];
    try 
    {
        srcFile.open(srcFileName.c_str(), std::ios_base::binary);
        if (!srcFile.is_open())
            return -2;
    
        srcFile.read(buffer, IO_BUFFER_SIZE);
        while (srcFile.gcount() > 0) 
        {
            dstFile.write(buffer, srcFile.gcount());
            srcFile.read(buffer, IO_BUFFER_SIZE);
        }
        srcFile.close();
        delete [] buffer;
        return 0;
    }
    catch(...) {
        delete [] buffer;
        return -2;
    }
}

wstring closeDirPath(const wstring& name)
{
    if (name.empty())
        return name;
    else if (name[name.length()-1] == L'/')
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
    if (value[value.length()] != L'/')
        value += L'/';
    value += fileName;

    return value;
}
int writeIndex(ofstream& dstFile, const vector<int64_t>& filePosList, vector<wstring>& fileNameList)
{
    int64_t indexStartPos = dstFile.tellp();
    for (int i = 1; i < filePosList.size(); ++i)
    {
        dstFile.write((const char*) &filePosList[i-1], sizeof(int64_t)); // no marsahling is required because of platform depending executable file
        int strLen = fileNameList[i].length();
        dstFile.write((const char*) &strLen, sizeof(int));
        dstFile.write((const char*) fileNameList[i].c_str(), strLen*sizeof(wchar_t));
    }

    //dstFile.write((const char*) &MAGIC, sizeof(int64_t)); // nov file posf
    //dstFile.write((const char*) &filePosList[filePosList.size()-2], sizeof(int64_t)); // nov file start
    dstFile.write((const char*) &indexStartPos, sizeof(int64_t));

    return 0;
}

void getSrcFileList(vector<wstring>& result, const wstring& srcDataFolder, const wstring& root)
{
    DIR *pDIR;
    struct dirent *entry;
    if( pDIR=opendir(srcDataFolder.c_str()))
    {
        while(entry = readdir(pDIR))
        {
            wstring name(entry->d_name);
            if(name !=  L"." != 0 && name != L"..")
            {
                if (entry->d_type == DT_DIR)
                    getSrcFileList(result, getFullFileName(srcDataFolder, name), closeDirPath(getFullFileName(root, name)));
                else 
                    result.push_back(root + name);
            }
        }
        closedir(pDIR);
    }
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

    struct dirent *entry;
    DIR *pDIR;
    if( pDIR=opendir(normalizedName.c_str()))
        closedir(pDIR);
    else
        createDirectory(normalizedName);
}

int extractFile(ifstream& srcFile, const wstring& fullFileName, int64_t pos, int64_t fileSize)
{
    //wstring fullFileName = getFullFileName(dstDir, fileName);
    //createDirectory(extractFilePath(fullFileName));

    srcFile.seekg(pos);
    ofstream dstFile;
    dstFile.open(fullFileName.c_str(), std::ios_base::binary);
    if (!dstFile.is_open())
        return -1;
    char* buffer = new char[1024*1024];
    int64_t bytesLeft = fileSize;
    
    do {
        srcFile.read(buffer, min(bytesLeft, 1024*1024));
        dstFile.write(buffer, srcFile.gcount());
        bytesLeft -= srcFile.gcount();
    } while (srcFile.gcount() > 0 && bytesLeft > 0);

    dstFile.close();
    delete [] buffer;
    return 0;
}

void startProcessAsync(wchar_t* commandline, const wstring& dstDir)
{
    STARTUPINFO lpStartupInfo;
    PROCESS_INFORMATION lpProcessInfo;
    memset(&lpStartupInfo, 0, sizeof(lpStartupInfo));
    memset(&lpProcessInfo, 0, sizeof(lpProcessInfo));
    CreateProcess(0, commandline,
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

        wchar_t buffer[MAX_PATH*2 +3];
        wsprintf(buffer, L"\"%s\" \"%s\"", getFullFileName(dstDir, L"client").c_str(), executePath.c_str());
        startProcessAsync(buffer, dstDir);

        return 0;
    } catch(...)
    {
        delete [] buffer;
        return -2;
    }
}

int createLaunchingFile(const wstring& launcherFile, const wstring& srcDataFolder, const wstring& novFileName, const wstring& dstName)
{
    vector<int64_t> filePosList;
    vector<wstring> fileNameList;
    vector<wstring> srcMediaFiles;
    getSrcFileList(srcMediaFiles, srcDataFolder, wstring());

    ofstream dstFile;
    try {
        dstFile.open(dstName.c_str(), std::ios_base::binary);
    }
    catch(...) {
        return -1;
    }

    if (appendFile(dstFile, launcherFile) != 0)
        return -2;
    filePosList.push_back(dstFile.tellp());
    fileNameList.push_back(launcherFile);

    for (int i= 0; i < srcMediaFiles.size(); ++i)
    {
        if (appendFile(dstFile, getFullFileName(srcDataFolder, srcMediaFiles[i])) == 0)
        {
            filePosList.push_back(dstFile.tellp());
            fileNameList.push_back(srcMediaFiles[i]);
        }
    }
    //if (appendFile(dstFile, novFileName) != 0)
    //    return -3;
    filePosList.push_back(dstFile.tellp());
    fileNameList.push_back(novFileName);

    if (writeIndex(dstFile, filePosList, fileNameList) != 0)
        return -4;

    int64_t novPos = dstFile.tellp();
    if (appendFile(dstFile, novFileName) != 0)
        return -3;

    dstFile.write((const char*) &novPos, sizeof(int64_t)); // nov file start
    dstFile.write((const char*) &MAGIC, sizeof(int64_t)); // magic

    dstFile.close();
    return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
    if (argc == 1)
    {
        launchFile(argv[0]);
    }
    else if (argc == 2)
    {
        launchFile(argv[1]);
    }
    else if (argc == 4)
    {
        return createLaunchingFile(argv[0], argv[1], argv[2], argv[3]);
    }

	return 0;
}
