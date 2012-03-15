// NetworkCA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <comutil.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <msxml2.h>
#include <winsock2.h>
#include <Ws2tcpip.h>

TCHAR* GetProperty(MSIHANDLE hInstall, LPCWSTR name)
{
    TCHAR* szValueBuf = NULL;

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

int NextFreePort(int startPort, int endPort)
{
    unsigned short port;
    for (port = startPort; port < endPort; port++) {
        if (IsPortAvailable(port))
            return port;
    }

    if (port == endPort) {
        int serverfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in channel;
        memset(&channel, 0, sizeof(channel));
        channel.sin_family = AF_INET;
        channel.sin_addr.s_addr = INADDR_ANY;

        bind(serverfd, (sockaddr *) &channel, sizeof(channel));

        socklen_t channellen;
        getsockname(serverfd, (sockaddr*) &channel, &channellen);

        port = ntohs(channel.sin_port);
        closesocket(serverfd);
    }

    return port;
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

UINT __stdcall CheckPorts(MSIHANDLE hInstall)
{
    CAtlString portsString;
    CAtlString portString;

    InitWinsock();

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CheckIfPortFree");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    portsString = GetProperty(hInstall, L"PORTSTOTEST");
    int curPos= 0;

    bool foundBusyPort = false;
    portString = portsString.Tokenize(L" ",curPos);
    while (portString != L"")
    {
        int port = _wtoi(portString);

        if (!IsPortAvailable(port)) {
            MsiSetProperty(hInstall, L"BUSYPORT", portString);
            foundBusyPort = true;
            break;
        }

        portString = portString.Tokenize(L" ",curPos);
    }

    if (!foundBusyPort)
        MsiSetProperty(hInstall, L"BUSYPORT", L"");

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall FindFreePorts(MSIHANDLE hInstall)
{
    InitWinsock();

    static const int PORT_BASE = 7001;

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "FindFreePorts");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    int freePort1 = NextFreePort(PORT_BASE, PORT_BASE + 100);
    int freePort2 = NextFreePort(PORT_BASE + 100, PORT_BASE + 200);
    int freePort3 = NextFreePort(PORT_BASE + 200, PORT_BASE + 300);

    wchar_t portString[10];

    _itow_s(freePort1, portString, 10, 10);
    MsiSetProperty(hInstall, L"FREEPORT1", portString);

    _itow_s(freePort2, portString, 10, 10);
    MsiSetProperty(hInstall, L"FREEPORT2", portString);

    _itow_s(freePort3, portString, 10, 10);
    MsiSetProperty(hInstall, L"FREEPORT3", portString);

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

