#include "stdafx.h"

#include "Utils.h"

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


