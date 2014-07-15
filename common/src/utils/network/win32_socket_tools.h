/**********************************************************
* 11 jul 2014
* a.kolesnikov
***********************************************************/

#ifdef _WIN32

#ifndef WIN32_SOCKET_TOOLS_H
#define WIN32_SOCKET_TOOLS_H


/*!
    \return ERROR_SUCCESS on success, otherwise - error code
*/
DWORD GetTcpRow(
    u_short localPort,
    u_short remotePort,
    MIB_TCP_STATE state,
    __out PMIB_TCPROW row );

/*!
    \return ERROR_SUCCESS on success, otherwise - error code
*/
DWORD readTcpStat(
    PMIB_TCPROW row,
    StreamSocketInfo* const sockInfo );

#endif  //WIN32_SOCKET_TOOLS_H

#endif
