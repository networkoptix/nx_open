#include "Utils.h"

#include <vector>
#include <set>
#include <algorithm>

#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "Windows.h"
#include "VersionHelpers.h"
#include "UserEnv.h"
#include <winsock2.h>
#include "IPTypes.h"
#include "iphlpapi.h"

#include <comutil.h>
#include <shobjidl.h>
#include <Shellapi.h>
#include <shlobj.h>

#include "Msi.h"
#include "MsiQuery.h"

#include "wcautil.h"
#include "fileutil.h"
#include "memutil.h"
#include "strutil.h"
#include "portchecker.h"

Error::Error(LPCWSTR msg)
    : _msg(msg) {}

LPCWSTR Error::msg() const {
    return (LPCWSTR)_msg;
}

#define VERIFY(exp, msg) ( if((exp) != ERROR_SUCCESS) { throw Error(msg); } )

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
    PortChecker port_checker;
    return port_checker.isPortAvailable(port);
}

bool IsPortRangeAvailable(int firstPort, int count)
{
    PortChecker port_checker;

    for (int port = firstPort; count; port++, count--)
        if (!port_checker.isPortAvailable(port))
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

  // Create destination directory if not exists
  if (GetFileAttributes(refcstrDestinationDirectory) == INVALID_FILE_ATTRIBUTES) {
      if(::SHCreateDirectoryEx(0, refcstrDestinationDirectory, 0) != ERROR_SUCCESS)
        return ::GetLastError();
  }

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
          if(CopyDirectory(strSource, strDestination))
            return ::GetLastError();
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

UINT CopyProfile(MSIHANDLE hInstall, const char* actionName, BOOL verifyDestFolderExists) {
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

//    Wow64DisableWow64FsRedirection(0);

    CAtlString foldersString, fromFolder, toFolder;

    hr = WcaInitialize(hInstall, actionName);
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    // Get "from" and "to" folders from msi property
    foldersString = GetProperty(hInstall, L"CustomActionData");

    // Extract "from" and "to" folders from foldersString
    int curPos = 0;
    fromFolder = foldersString.Tokenize(_T(";"), curPos);
    toFolder = foldersString.Tokenize(_T(";"), curPos);

    // Exit if "from" folder is not exists
    if (GetFileAttributes(fromFolder) == INVALID_FILE_ATTRIBUTES)
        goto LExit;

    if (verifyDestFolderExists) {
        // Exit if "to" folder is already exists
        if (GetFileAttributes(toFolder) != INVALID_FILE_ATTRIBUTES)
            goto LExit;
    }

    CopyDirectory(fromFolder, toFolder);

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

void fixPath(CString& path)
{
    path.Replace(L"/", L"\\");
}

#define OUTPUT_BUFFER 1024

static HRESULT LogOutput(
    __in HANDLE hRead
    )
{
    BYTE *pBuffer = NULL;
    LPWSTR szLog = NULL;
    LPWSTR szTemp = NULL;
    LPWSTR pEnd = NULL;
    LPWSTR pNext = NULL;
    LPWSTR sczEscaped = NULL;
    LPSTR szWrite = NULL;
    DWORD dwBytes = OUTPUT_BUFFER;
    BOOL bFirst = TRUE;
    BOOL bUnicode = TRUE;
    HRESULT hr = S_OK;

    // Get buffer for output
    pBuffer = static_cast<BYTE *>(MemAlloc(OUTPUT_BUFFER, FALSE));
    ExitOnNull(pBuffer, hr, E_OUTOFMEMORY, "Failed to allocate buffer for output.");

    while (0 != dwBytes)
    {
        ::ZeroMemory(pBuffer, OUTPUT_BUFFER);
        if (!::ReadFile(hRead, pBuffer, OUTPUT_BUFFER - 1, &dwBytes, NULL) && GetLastError() != ERROR_BROKEN_PIPE)
        {
            ExitOnLastError(hr, "Failed to read from handle.");
        }

        // Check for UNICODE or ANSI output
        if (bFirst)
        {
            if ((isgraph(pBuffer[0]) && isgraph(pBuffer[1])) ||
                (isgraph(pBuffer[0]) && isspace(pBuffer[1])) ||
                (isspace(pBuffer[0]) && isgraph(pBuffer[1])) ||
                (isspace(pBuffer[0]) && isspace(pBuffer[1])))
            {
                bUnicode = FALSE;
            }

            bFirst = FALSE;
        }

        // Keep track of output
        if (bUnicode)
        {
            hr = StrAllocConcat(&szLog, (LPCWSTR)pBuffer, 0);
            ExitOnFailure(hr, "failed to concatenate output strings");
        }
        else
        {
            hr = StrAllocStringAnsi(&szTemp, (LPCSTR)pBuffer, 0, CP_OEMCP);
            ExitOnFailure(hr, "failed to allocate output string");
            hr = StrAllocConcat(&szLog, szTemp, 0);
            ExitOnFailure(hr, "failed to concatenate output strings");
        }

        // Log each line of the output
        pNext = szLog;
        pEnd = wcschr(szLog, L'\r');
        if (NULL == pEnd)
        {
            pEnd = wcschr(szLog, L'\n');
        }
        while (pEnd && *pEnd)
        {
            // Find beginning of next line
            pEnd[0] = 0;
            ++pEnd;
            if ((pEnd[0] == L'\r') || (pEnd[0] == L'\n'))
            {
                ++pEnd;
            }

            // Log output
            hr = StrAllocString(&sczEscaped, pNext, 0);
            ExitOnFailure(hr, "Failed to allocate copy of string");

            hr = StrReplaceStringAll(&sczEscaped, L"%", L"%%");
            ExitOnFailure(hr, "Failed to escape percent signs in string");

            hr = StrAnsiAllocString(&szWrite, sczEscaped, 0, CP_OEMCP);
            ExitOnFailure(hr, "failed to convert output to ANSI");
            WcaLog(LOGMSG_STANDARD, szWrite);

            // Next line
            pNext = pEnd;
            pEnd = wcschr(pNext, L'\r');
            if (NULL == pEnd)
            {
                pEnd = wcschr(pNext, L'\n');
            }
        }

        hr = StrAllocString(&szTemp, pNext, 0);
        ExitOnFailure(hr, "failed to allocate string");

        hr = StrAllocString(&szLog, szTemp, 0);
        ExitOnFailure(hr, "failed to allocate string");
    }

    // Print any text that didn't end with a new line
    if (szLog && *szLog)
    {
        hr = StrReplaceStringAll(&szLog, L"%", L"%%");
        ExitOnFailure(hr, "Failed to escape percent signs in string");

        hr = StrAnsiAllocString(&szWrite, szLog, 0, CP_OEMCP);
        ExitOnFailure(hr, "failed to convert output to ANSI");

        WcaLog(LOGMSG_VERBOSE, szWrite);
    }

LExit:
    ReleaseMem(pBuffer);

    ReleaseStr(szLog);
    ReleaseStr(szTemp);
    ReleaseStr(szWrite);
    ReleaseStr(sczEscaped);

    return hr;
}

static HRESULT CreatePipes(
    __out HANDLE *phOutRead,
    __out HANDLE *phOutWrite,
    __out HANDLE *phErrWrite,
    __out HANDLE *phInRead,
    __out HANDLE *phInWrite
    )
{
    Assert(phOutRead);
    Assert(phOutWrite);
    Assert(phErrWrite);
    Assert(phInRead);
    Assert(phInWrite);

    HRESULT hr = S_OK;
    SECURITY_ATTRIBUTES sa;
    HANDLE hOutTemp = INVALID_HANDLE_VALUE;
    HANDLE hInTemp = INVALID_HANDLE_VALUE;

    HANDLE hOutRead = INVALID_HANDLE_VALUE;
    HANDLE hOutWrite = INVALID_HANDLE_VALUE;
    HANDLE hErrWrite = INVALID_HANDLE_VALUE;
    HANDLE hInRead = INVALID_HANDLE_VALUE;
    HANDLE hInWrite = INVALID_HANDLE_VALUE;

    // Fill out security structure so we can inherit handles
    ::ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes
    if (!::CreatePipe(&hOutTemp, &hOutWrite, &sa, 0))
    {
        ExitOnLastError(hr, "failed to create output pipe");
    }

    if (!::CreatePipe(&hInRead, &hInTemp, &sa, 0))
    {
        ExitOnLastError(hr, "failed to create input pipe");
    }


    // Duplicate output pipe so standard error and standard output write to
    // the same pipe
    if (!::DuplicateHandle(::GetCurrentProcess(), hOutWrite, ::GetCurrentProcess(), &hErrWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
        ExitOnLastError(hr, "failed to duplicate write handle");
    }

    // We need to create new output read and input write handles that are
    // non inheritable.  Otherwise it creates handles that can't be closed.
    if (!::DuplicateHandle(::GetCurrentProcess(), hOutTemp, ::GetCurrentProcess(), &hOutRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        ExitOnLastError(hr, "failed to duplicate output pipe");
    }

    if (!::DuplicateHandle(::GetCurrentProcess(), hInTemp, ::GetCurrentProcess(), &hInWrite, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        ExitOnLastError(hr, "failed to duplicate input pipe");
    }

    // now that everything has succeeded, assign to the outputs
    *phOutRead = hOutRead;
    hOutRead = INVALID_HANDLE_VALUE;

    *phOutWrite = hOutWrite;
    hOutWrite = INVALID_HANDLE_VALUE;

    *phErrWrite = hErrWrite;
    hErrWrite = INVALID_HANDLE_VALUE;

    *phInRead = hInRead;
    hInRead = INVALID_HANDLE_VALUE;

    *phInWrite = hInWrite;
    hInWrite = INVALID_HANDLE_VALUE;

LExit:
    ReleaseFile(hOutRead);
    ReleaseFile(hOutWrite);
    ReleaseFile(hErrWrite);
    ReleaseFile(hInRead);
    ReleaseFile(hInWrite);
    ReleaseFile(hOutTemp);
    ReleaseFile(hInTemp);

    return hr;
}

HRESULT WIXAPI QuietExecWithReturnCode(
    __inout_z LPWSTR wzCommand,
    __in DWORD dwTimeout,
    __out DWORD& dwExitCode
    )
{
    HRESULT hr = S_OK;
    PROCESS_INFORMATION oProcInfo;
    STARTUPINFOW oStartInfo;
    HANDLE hOutRead = INVALID_HANDLE_VALUE;
    HANDLE hOutWrite = INVALID_HANDLE_VALUE;
    HANDLE hErrWrite = INVALID_HANDLE_VALUE;
    HANDLE hInRead = INVALID_HANDLE_VALUE;
    HANDLE hInWrite = INVALID_HANDLE_VALUE;

    dwExitCode = ERROR_SUCCESS;

    memset(&oProcInfo, 0, sizeof(oProcInfo));
    memset(&oStartInfo, 0, sizeof(oStartInfo));

    // Create output redirect pipes
    hr = CreatePipes(&hOutRead, &hOutWrite, &hErrWrite, &hInRead, &hInWrite);
    ExitOnFailure(hr, "failed to create output pipes");

    // Set up startup structure
    oStartInfo.cb = sizeof(STARTUPINFOW);
    oStartInfo.dwFlags = STARTF_USESTDHANDLES;
    oStartInfo.hStdInput = hInRead;
    oStartInfo.hStdOutput = hOutWrite;
    oStartInfo.hStdError = hErrWrite;

    WcaLog(LOGMSG_VERBOSE, "%ls", wzCommand);

#pragma prefast(suppress:25028)
    if (::CreateProcessW(NULL,
        wzCommand, // command line
        NULL, // security info
        NULL, // thread info
        TRUE, // inherit handles
        ::GetPriorityClass(::GetCurrentProcess()) | CREATE_NO_WINDOW, // creation flags
        NULL, // environment
        NULL, // cur dir
        &oStartInfo,
        &oProcInfo))
    {
        ReleaseFile(oProcInfo.hThread);

        // Close child output/input handles so it doesn't hang
        ReleaseFile(hOutWrite);
        ReleaseFile(hErrWrite);
        ReleaseFile(hInRead);

        // Log output
        LogOutput(hOutRead);

        // Wait for everything to finish
        ::WaitForSingleObject(oProcInfo.hProcess, dwTimeout);
        if (!::GetExitCodeProcess(oProcInfo.hProcess, &dwExitCode))
        {
            dwExitCode = ERROR_SEM_IS_SET;
        }

        ReleaseFile(hOutRead);
        ReleaseFile(hInWrite);
        ReleaseFile(oProcInfo.hProcess);
    }
    else
    {
        ExitOnLastError(hr, "Command failed to execute.");
    }

    ExitOnWin32Error(dwExitCode, hr, "Command line returned an error.");

LExit:
    return hr;
}

void QuitExecAndWarn(const LPWSTR commandLine, int status, const LPWSTR warningMsg) {
    DWORD commandStatus;
    QuietExecWithReturnCode(commandLine, 0, commandStatus);
    if (commandStatus == status) {
        MessageBox(0, warningMsg, L"Warning!", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
    }
}

#define MAX_TRIES 3
#define WORKING_BUFFER_SIZE 15000

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

typedef std::set<unsigned long> addresses_t;

bool fill_local_addresses(addresses_t& local_addresses) {
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do {

        pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
        if (pAddresses == NULL) {
            return false;
        }

        dwRetVal =
            GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_MULTICAST, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            FREE(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }

        Iterations++;

    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR) {
        pCurrAddresses = pAddresses;
        for (; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;

            if (pCurrAddresses->OperStatus != IfOperStatusUp) {
                continue;
            }

            pUnicast = pCurrAddresses->FirstUnicastAddress;
            if (pUnicast != NULL) {
                for (int i = 0; pUnicast != NULL; i++) {
                    local_addresses.insert(((SOCKADDR_IN *)(pUnicast->Address.lpSockaddr))->sin_addr.S_un.S_addr);
                    // cout << inet_ntoa(((SOCKADDR_IN *)(pUnicast->Address.lpSockaddr))->sin_addr) << endl;
                    pUnicast = pUnicast->Next;
                }
            }
        }
    }

    if (pAddresses) {
        FREE(pAddresses);
    }

    return true;
}

bool isStandaloneSystem(const char* host) {
    struct hostent *phe = gethostbyname(host);
    if (phe == 0) {
        // Can not resolve host => system is not standalone
        return false;
    }

    addresses_t host_addresses;
    for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
        struct in_addr addr;
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
        host_addresses.insert(addr.S_un.S_addr);
    }

    addresses_t local_addresses;
    fill_local_addresses(local_addresses);

    std::vector<unsigned long> intersection;
    std::set_intersection(host_addresses.begin(), host_addresses.end(), local_addresses.begin(), local_addresses.end(), std::back_inserter(intersection));
    return !intersection.empty();

}

BOOL Is64BitWindows()
{
#if defined(_WIN64)
 return TRUE;  // 64-bit programs run only on Win64
#elif defined(_WIN32)
 // 32-bit programs run on both 32-bit and 64-bit Windows
 // so must sniff
 BOOL f64 = FALSE;
 return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
 return FALSE; // Win64 does not support Win16
#endif
}

CString GetAppDataLocalFolderPath() {
    TCHAR buffer[MAX_PATH*2] = { 0 };
    DWORD size = MAX_PATH * 2;

    CAtlString result;

    if (IsWindowsVistaOrGreater()) {
        SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, buffer);
        result.Format(L"%s\\config\\systemprofile\\AppData\\Local", buffer);
    } else {
        GetProfilesDirectory(buffer, &size);
        CAtlString user = Is64BitWindows() ? L"Default User" : L"LocalService";
        result.Format(L"%s\\%s\\Local Settings\\Application Data", buffer, user);
    }

    return result;
}
