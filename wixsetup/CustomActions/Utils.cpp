#include "stdafx.h"

#include "Utils.h"

CString GenerateGuid()
{
    static const int MAX_GUID_SIZE = 50;

    GUID guid;
    WCHAR guidBuffer[MAX_GUID_SIZE]; 
    CoCreateGuid(&guid);
    StringFromGUID2(guid, guidBuffer, MAX_GUID_SIZE);

    return CString(guidBuffer).MakeLower();
}

LPCWSTR GetProperty(MSIHANDLE hInstall, LPCWSTR name)
{
    LPWSTR szValueBuf = NULL;

    DWORD dwSize=0;
    UINT uiStat = MsiGetProperty(hInstall, name, TEXT(""), &dwSize);
    if (ERROR_MORE_DATA == uiStat && dwSize != 0)
    {
        dwSize++; // add 1 for terminating '\0'
        szValueBuf = new TCHAR[dwSize];
        uiStat = MsiGetProperty(hInstall, name, szValueBuf, &dwSize);
    }

    if (ERROR_SUCCESS != uiStat)
    {
        if (szValueBuf != NULL) 
            delete[] szValueBuf;

        return NULL;
    }

    return szValueBuf;
}

bool IsPortAvailable(int port)
{
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in channel;
    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = INADDR_ANY;
    channel.sin_port = htons(port);

    int bind_status = bind(serverfd, (sockaddr *) &channel, sizeof(channel));
    closesocket(serverfd);

    return bind_status == 0;
}

bool IsPortRangeAvailable(int firstPort, int count)
{
    for (int port = firstPort; count; port++, count--)
        if (!IsPortAvailable(port))
            return false;

    return true;
}

void InitWinsock()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void FinishWinsock()
{
    WSACleanup();
}

int CopyDirectory(const CAtlString &refcstrSourceDirectory,
                  const CAtlString &refcstrDestinationDirectory)
{
  CAtlString     strSource;               // Source file
  CAtlString     strDestination;          // Destination file
  CAtlString     strPattern;              // Pattern
  HANDLE          hFile;                   // Handle to file
  WIN32_FIND_DATA FileInformation;         // File information


  strPattern = refcstrSourceDirectory + "\\*";

  // Create destination directory
  if(::CreateDirectory(refcstrDestinationDirectory, 0) == FALSE)
    return ::GetLastError();

  hFile = ::FindFirstFile(strPattern, &FileInformation);
  if(hFile != INVALID_HANDLE_VALUE)
  {
    do
    {
      if(FileInformation.cFileName[0] != '.')
      {
        strSource      = refcstrSourceDirectory + "\\" + FileInformation.cFileName;
        strDestination = refcstrDestinationDirectory + "\\" + FileInformation.cFileName;

        if(FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          // Copy subdirectory
          return CopyDirectory(strSource, strDestination);
        }
        else
        {
          // Copy file
          if(::CopyFile(strSource, strDestination, TRUE) == FALSE)
            return ::GetLastError();
        }
      }
    } while(::FindNextFile(hFile, &FileInformation) == TRUE);

    // Close handle
    ::FindClose(hFile);

    DWORD dwError = ::GetLastError();
    if(dwError != ERROR_NO_MORE_FILES)
      return dwError;
  }

  return 0;
}

