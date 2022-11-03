/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the
above copyright notice, this list of conditions
and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
nor the names of its contributors may be used to
endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
Yunhong Gu, last updated 01/18/2011
*****************************************************************************/

#pragma once

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#ifdef __MINGW__
#include <stdint.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>
#include <winsock2.h>
#endif
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>


////////////////////////////////////////////////////////////////////////////////

//if compiling on VC6.0 or pre-WindowsXP systems
//use -DLEGACY_WIN32

//if compiling with MinGW, it only works on XP or above
//use -D_WIN32_WINNT=0x0501


#ifdef _WIN32
#ifndef __MINGW__
// Explicitly define 32-bit and 64-bit numbers
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int32 uint32_t;
#ifndef LEGACY_WIN32
typedef unsigned __int64 uint64_t;
#else
// VC 6.0 does not support unsigned __int64: may cause potential problems.
typedef __int64 uint64_t;
#endif
#endif
#endif

#define NO_BUSY_WAITING

#ifdef _WIN32
#ifndef __MINGW__
typedef SOCKET SYSSOCKET;
#else
typedef int SYSSOCKET;
#endif
#else
typedef int SYSSOCKET;
#endif

typedef SYSSOCKET UDPSOCKET;
typedef int UDTSOCKET;

#ifdef _WIN32
#define INVALID_UDP_SOCKET INVALID_SOCKET
#else
#define INVALID_UDP_SOCKET -1
#endif

////////////////////////////////////////////////////////////////////////////////

typedef std::set<UDTSOCKET> ud_set;
#define UD_CLR(u, uset) ((uset)->erase(u))
#define UD_ISSET(u, uset) ((uset)->find(u) != (uset)->end())
#define UD_SET(u, uset) ((uset)->insert(u))
#define UD_ZERO(uset) ((uset)->clear())

enum EPOLLOpt
{
    // this values are defined same as linux epoll.h
    // so that if system values are used by mistake, they should have the same effect
    UDT_EPOLL_IN = 0x1,
    UDT_EPOLL_OUT = 0x4,
    UDT_EPOLL_ERR = 0x8,
    UDT_EPOLL_HUP = 0x10,
    UDT_EPOLL_RDHUP = 0x2000
};

enum UDTSTATUS { INIT = 1, OPENED, LISTENING, CONNECTING, CONNECTED, BROKEN, CLOSING, CLOSED, NONEXIST };

////////////////////////////////////////////////////////////////////////////////

enum UDTOpt
{
    UDT_MSS,             // the Maximum Transfer Unit
    UDT_SNDSYN,          // if sending is blocking
    UDT_RCVSYN,          // if receiving is blocking
    UDT_FC,        // Flight flag size (window size)
    UDT_SNDBUF,          // maximum buffer in sending queue
    UDT_RCVBUF,          // UDT receiving buffer size
    UDT_LINGER,          // waiting for unsent data when closing
    UDP_SNDBUF,          // UDP sending buffer size
    UDP_RCVBUF,          // UDP receiving buffer size
    UDT_MAXMSG,          // maximum datagram message size
    UDT_MSGTTL,          // time-to-live of a datagram message
    UDT_RENDEZVOUS,      // rendezvous connection mode
    UDT_SNDTIMEO,        // send() timeout
    UDT_RCVTIMEO,        // recv() timeout
    UDT_REUSEADDR,    // reuse an existing port or create a new one
    UDT_MAXBW,        // maximum bandwidth (bytes per second) that the connection can use
    UDT_STATE,        // current socket state, see UDTSTATUS, read only
    UDT_EVENT,        // current avalable events associated with the socket
    UDT_SNDDATA,        // size of data in the sending buffer
    UDT_RCVDATA        // size of data available for recv
};

enum UDTShutdownOpt
{
#if defined(_WIN32)
    UDT_SHUT_RDWR = SD_BOTH
#else
    UDT_SHUT_RDWR = SHUT_RDWR
#endif
};

////////////////////////////////////////////////////////////////////////////////

struct CPerfMon
{
    // global measurements

    // time since the UDT entity is started, in milliseconds.
    std::chrono::milliseconds msTimeStamp;
    int64_t pktSentTotal;                // total number of sent data packets, including retransmissions
    int64_t pktRecvTotal;                // total number of received packets
    int pktSndLossTotal;                 // total number of lost packets (sender side)
    int pktRcvLossTotal;                 // total number of lost packets (receiver side)
    int pktRetransTotal;                 // total number of retransmitted packets
    int pktSentACKTotal;                 // total number of sent ACK packets
    int pktRecvACKTotal;                 // total number of received ACK packets
    int pktSentNAKTotal;                 // total number of sent NAK packets
    int pktRecvNAKTotal;                 // total number of received NAK packets
    std::chrono::microseconds usSndDurationTotal;        // total time duration when UDT is sending data (idle time exclusive)

                                    // local measurements
    int64_t pktSent;                     // number of sent data packets, including retransmissions
    int64_t pktRecv;                     // number of received packets
    int pktSndLoss;                      // number of lost packets (sender side)
    int pktRcvLoss;                      // number of lost packets (receiver side)
    int pktRetrans;                      // number of retransmitted packets
    int pktSentACK;                      // number of sent ACK packets
    int pktRecvACK;                      // number of received ACK packets
    int pktSentNAK;                      // number of sent NAK packets
    int pktRecvNAK;                      // number of received NAK packets
    double mbpsSendRate;                 // sending rate in Mb/s
    double mbpsRecvRate;                 // receiving rate in Mb/s
    std::chrono::microseconds usSndDuration;        // busy sending time (i.e., idle time exclusive)

                                // instant measurements
    double usPktSndPeriod;               // packet sending period, in microseconds
    int pktFlowWindow;                   // flow window size, in number of packets
    int pktCongestionWindow;             // congestion window size, in number of packets
    int pktFlightSize;                   // number of packets on flight
    std::chrono::microseconds rtt;
    double mbpsBandwidth;                // estimated bandwidth, in Mb/s
    int byteAvailSndBuf;                 // available UDT sender buffer size
    int byteAvailRcvBuf;                 // available UDT receiver buffer size
};

////////////////////////////////////////////////////////////////////////////////

// If you need to export these APIs to be used by a different language,
// declare extern "C" for them, and add a "udt_" prefix to each API.
// The following APIs: sendfile(), recvfile(), epoll_wait(), geterrormsg(),
// include C++ specific feature, please use the corresponding sendfile2(), etc.

namespace UDT {

enum class ProtocolError {
    none = 0,
    cannotListenInRendezvousMode,
    handshakeFailure,
    remotePeerInRendezvousMode,
    retransmitReceived,
    outOfWindowDataReceived,
};

UDT_API std::string toString(ProtocolError);

#ifdef _WIN32
using Errno = DWORD;
#else
using Errno = int;
#endif

//-------------------------------------------------------------------------------------------------

class UDT_API Error
{
public:
    /**
     * Sets osError() to the last OS error code.
     */
    Error(ProtocolError protocolError = ProtocolError::none);

    Error(Errno osError, ProtocolError protocolError = ProtocolError::none);

    Errno osError() const;
    ProtocolError protocolError() const;
    const char* errorText() const;

private:
    Errno m_osError;
    ProtocolError m_protocolError;
    std::string m_errorText;

    std::string prepareErrorText();
};

//-------------------------------------------------------------------------------------------------

typedef UDTOpt SOCKOPT;
typedef CPerfMon TRACEINFO;
typedef ud_set UDSET;

UDT_API extern const UDTSOCKET INVALID_SOCK;
#undef ERROR
UDT_API extern const int ERROR;

UDT_API int startup();
UDT_API int cleanup();
UDT_API UDTSOCKET socket(int af, int type, int protocol);
UDT_API int bind(UDTSOCKET u, const struct sockaddr* name, int namelen);
UDT_API int bind2(UDTSOCKET u, UDPSOCKET udpsock);
UDT_API int listen(UDTSOCKET u, int backlog);
UDT_API UDTSOCKET accept(UDTSOCKET u, struct sockaddr* addr, int* addrlen);
UDT_API int connect(UDTSOCKET u, const struct sockaddr* name, int namelen);

/**
 * @param how. Currently ignored. The function always works as if UDT_SHUT_RDWR was passed here.
 */
UDT_API int shutdown(UDTSOCKET u, int how);
UDT_API int close(UDTSOCKET u);
UDT_API int getpeername(UDTSOCKET u, struct sockaddr* name, int* namelen);
UDT_API int getsockname(UDTSOCKET u, struct sockaddr* name, int* namelen);
UDT_API int getsockopt(UDTSOCKET u, int level, SOCKOPT optname, void* optval, int* optlen);
UDT_API int setsockopt(UDTSOCKET u, int level, SOCKOPT optname, const void* optval, int optlen);
UDT_API int send(UDTSOCKET u, const char* buf, int len, int flags);
UDT_API int recv(UDTSOCKET u, char* buf, int len, int flags);
UDT_API int sendmsg(UDTSOCKET u, const char* buf, int len, int ttl = -1, bool inorder = false);
UDT_API int recvmsg(UDTSOCKET u, char* buf, int len);
UDT_API int64_t sendfile(UDTSOCKET u, std::fstream& ifs, int64_t& offset, int64_t size, int block = 364000);
UDT_API int64_t recvfile(UDTSOCKET u, std::fstream& ofs, int64_t& offset, int64_t size, int block = 7280000);
UDT_API int64_t sendfile2(UDTSOCKET u, const char* path, int64_t* offset, int64_t size, int block = 364000);
UDT_API int64_t recvfile2(UDTSOCKET u, const char* path, int64_t* offset, int64_t size, int block = 7280000);

// select and selectEX are DEPRECATED; please use epoll.
UDT_API int select(int nfds, UDSET* readfds, UDSET* writefds, UDSET* exceptfds, const struct timeval* timeout);
UDT_API int selectEx(const std::vector<UDTSOCKET>& fds, std::vector<UDTSOCKET>* readfds,
    std::vector<UDTSOCKET>* writefds, std::vector<UDTSOCKET>* exceptfds, int64_t msTimeOut);

UDT_API int epoll_create();
UDT_API int epoll_add_usock(int eid, UDTSOCKET u, const int* events = NULL);
UDT_API int epoll_add_ssock(int eid, SYSSOCKET s, const int* events = NULL);
UDT_API int epoll_remove_usock(int eid, UDTSOCKET u);
UDT_API int epoll_remove_ssock(int eid, SYSSOCKET s);

/**
 * @param readfds map<socket handle, event mask (bit mask of EPOLLOpt values)>.
 * UDT_EPOLL_IN in implied. Event mask should not be tested for UDT_EPOLL_IN.
 * Same rules apply to each fd set.
 */
UDT_API int epoll_wait(
    int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds,
    int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds = NULL, std::map<SYSSOCKET, int>* wrfds = NULL);

UDT_API int epoll_wait2(
    int eid,
    UDTSOCKET* readfds, int* rnum,
    UDTSOCKET* writefds, int* wnum,
    int64_t msTimeOut,
    SYSSOCKET* lrfds = NULL, int* lrnum = NULL,
    SYSSOCKET* lwfds = NULL, int* lwnum = NULL);

/**
 * Interrupts one epoll_wait call: the one running simultaneously in another thread (if any)
 * or the next call to be made.
 * Causes epoll_wait to return 0 as if timeout has passed.
 */
UDT_API int epoll_interrupt_wait(int eid);
UDT_API int epoll_release(int eid);
UDT_API const Error& getlasterror();
UDT_API int perfmon(UDTSOCKET u, TRACEINFO* perf, bool clear = true);
UDT_API UDTSTATUS getsockstate(UDTSOCKET u);

}  // namespace UDT
