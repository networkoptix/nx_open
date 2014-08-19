/**********************************************************
* 11 jul 2014
* a.kolesnikov
***********************************************************/

#ifdef _WIN32

#include <Windows.h>

#include <memory>

#include "abstract_socket.h"


/*!
    \return error code
*/
DWORD GetTcpRow(
    u_short localPort,
    u_short remotePort,
    MIB_TCP_STATE state,
    __out PMIB_TCPROW row )
{
    PMIB_TCPTABLE tcpTable = NULL;
    PMIB_TCPROW tcpRowIt = NULL;

    DWORD status = 0, size = 0, i = 0;
    bool connectionFound = FALSE;

    status = GetTcpTable(tcpTable, &size, TRUE);
    if (status != ERROR_INSUFFICIENT_BUFFER) {
        return status;
    }

    tcpTable = (PMIB_TCPTABLE) malloc(size);
    if (tcpTable == NULL) {
        return ERROR_OUTOFMEMORY;
    }

    status = GetTcpTable(tcpTable, &size, TRUE);
    if (status != ERROR_SUCCESS) {
        free(tcpTable);
        return status;
    }

    localPort = ntohs(localPort);
    remotePort = ntohs(remotePort);

    for (i = 0; i < tcpTable->dwNumEntries; i++) {
        tcpRowIt = &tcpTable->table[i];
        if( tcpRowIt->dwLocalPort == (DWORD) localPort &&
            tcpRowIt->dwRemotePort == (DWORD) remotePort )
        {
            if(tcpRowIt->State == state)
            {
                connectionFound = TRUE;
                *row = *tcpRowIt;
                break;
            }
        }
    }

    free(tcpTable);

    if (connectionFound) {
        return ERROR_SUCCESS;
    } else {
        return ERROR_NOT_FOUND;
    }
}

DWORD readTcpStat(
    PMIB_TCPROW row,
    StreamSocketInfo* const sockInfo )
{
    auto freeLambda = [](void* ptr){ ::free(ptr); };
    std::unique_ptr<TCP_ESTATS_PATH_ROD_v0, decltype(freeLambda)> pathRod( (TCP_ESTATS_PATH_ROD_v0*)malloc( sizeof(TCP_ESTATS_PATH_ROD_v0) ), freeLambda );
    if( !pathRod.get() )
    {
        wprintf(L"\nOut of memory");
        return ERROR_OUTOFMEMORY;
    }

    memset( pathRod.get(), 0, sizeof(*pathRod) ); // zero the buffer
    ULONG winStatus = GetPerTcpConnectionEStats(
        row,
        TcpConnectionEstatsPath,
        NULL, 0, 0,
        NULL, 0, 0,
        (UCHAR*)pathRod.get(), 0, sizeof(*pathRod) );

    if( winStatus != NO_ERROR )
        return winStatus;

    sockInfo->rttVar = pathRod->RttVar;
    return ERROR_SUCCESS;
}

#endif
