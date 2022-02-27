// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    nx::network::StreamSocketInfo* const sockInfo );

#endif // _WIN32
