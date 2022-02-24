// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "Utils.h"

#include <vector>
#include <set>
#include <algorithm>
#include <iterator>

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
