#pragma once

#ifdef _WIN32

/**
 * @return ERROR_SUCCESS on success, otherwise - error code.
 */
DWORD NX_NETWORK_API GetTcpRow(
    u_short localPort,
    u_short remotePort,
    MIB_TCP_STATE state,
    __out PMIB_TCPROW row );

/**
 * @return ERROR_SUCCESS on success, otherwise - error code.
 */
DWORD NX_NETWORK_API readTcpStat(
    PMIB_TCPROW row,
    StreamSocketInfo* const sockInfo );

#endif // _WIN32
