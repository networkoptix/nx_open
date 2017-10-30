#ifdef _WIN32

#include <Windows.h>

#include <memory>

#include <nx/utils/platform/win32_syscall_resolver.h>

#include "abstract_socket.h"

DWORD GetTcpRow(
    u_short localPort,
    u_short remotePort,
    MIB_TCP_STATE state,
    __out PMIB_TCPROW row)
{
    // Dynamically resolving functions that require win >= vista we want to use here.
    typedef decltype(&GetTcpTable) GetTcpTableType;
    static GetTcpTableType GetTcpTableAddr =
        Win32FuncResolver::instance()->resolveFunction<GetTcpTableType>
        (L"Iphlpapi.dll", "GetTcpTable");

    if (GetTcpTableAddr == NULL)
        return ERROR_NOT_FOUND;

    PMIB_TCPTABLE tcpTable = NULL;
    PMIB_TCPROW tcpRowIt = NULL;

    DWORD status = 0, size = 0, i = 0;
    bool connectionFound = FALSE;

    status = GetTcpTableAddr(tcpTable, &size, TRUE);
    if (status != ERROR_INSUFFICIENT_BUFFER)
    {
        return status;
    }

    tcpTable = (PMIB_TCPTABLE)malloc(size);
    if (tcpTable == NULL)
    {
        return ERROR_OUTOFMEMORY;
    }

    status = GetTcpTableAddr(tcpTable, &size, TRUE);
    if (status != ERROR_SUCCESS)
    {
        free(tcpTable);
        return status;
    }

    localPort = ntohs(localPort);
    remotePort = ntohs(remotePort);

    for (i = 0; i < tcpTable->dwNumEntries; i++)
    {
        tcpRowIt = &tcpTable->table[i];
        if (tcpRowIt->dwLocalPort == (DWORD)localPort &&
            tcpRowIt->dwRemotePort == (DWORD)remotePort)
        {
            if (tcpRowIt->State == state)
            {
                connectionFound = TRUE;
                *row = *tcpRowIt;
                break;
            }
        }
    }

    free(tcpTable);

    if (connectionFound)
    {
        return ERROR_SUCCESS;
    }
    else
    {
        return ERROR_NOT_FOUND;
    }
}

DWORD readTcpStat(
    PMIB_TCPROW row,
    StreamSocketInfo* const sockInfo)
{
    //dynamically resolving functions that require win >= vista we want to use here
    typedef decltype(&GetPerTcpConnectionEStats) GetPerTcpConnectionEStatsType;
    static GetPerTcpConnectionEStatsType GetPerTcpConnectionEStatsAddr =
        Win32FuncResolver::instance()->resolveFunction<GetPerTcpConnectionEStatsType>
        (L"Iphlpapi.dll", "GetPerTcpConnectionEStats");

    if (GetPerTcpConnectionEStatsAddr == NULL)
        return ERROR_NOT_FOUND;

    auto freeLambda = [](void* ptr) { ::free(ptr); };
    std::unique_ptr<TCP_ESTATS_PATH_ROD_v0, decltype(freeLambda)> pathRod(
        (TCP_ESTATS_PATH_ROD_v0*)malloc(sizeof(TCP_ESTATS_PATH_ROD_v0)), freeLambda);
    if (!pathRod.get())
    {
        wprintf(L"\nOut of memory");
        return ERROR_OUTOFMEMORY;
    }

    memset(pathRod.get(), 0, sizeof(*pathRod)); // zero the buffer
    ULONG winStatus = GetPerTcpConnectionEStatsAddr(
        row,
        TcpConnectionEstatsPath,
        NULL, 0, 0,
        NULL, 0, 0,
        (UCHAR*)pathRod.get(), 0, sizeof(*pathRod));

    if (winStatus != NO_ERROR)
        return winStatus;

    sockInfo->rttVar = pathRod->RttVar;
    return ERROR_SUCCESS;
}

#endif //_WIN32
