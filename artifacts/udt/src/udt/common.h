/*****************************************************************************
Copyright (c) 2001 - 2009, The Board of Trustees of the University of Illinois.
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
Yunhong Gu, last updated 08/01/2009
*****************************************************************************/

#ifndef __UDT_COMMON_H__
#define __UDT_COMMON_H__

#include <chrono>
#include <cstring>
#include <memory>

#ifndef _WIN32
#include <sys/time.h>
#include <sys/uio.h>
#include <pthread.h>
#else
#include <windows.h>
#endif
#include <cstdlib>

#include "udt.h"
#include "socket_addresss.h"

#ifdef _WIN32
// Windows compability
typedef HANDLE pthread_t;
typedef HANDLE pthread_mutex_t;
typedef HANDLE pthread_cond_t;
typedef DWORD pthread_key_t;
#endif

#ifndef _WIN32
// Use these functions intead of POSIX to ensure monotonic waits on every UNIX platform:
int pthread_cond_init_monotonic(pthread_cond_t* condition);

int pthread_cond_wait_monotonic_timeout(
    pthread_cond_t* condition, pthread_mutex_t* mutex, uint64_t timeoutMks);

int pthread_cond_wait_monotonic_timepoint(
    pthread_cond_t* condition, pthread_mutex_t* mutex, uint64_t timeMks);
#endif

#ifdef _WIN32
using ThreadId = DWORD;
#else
using ThreadId = pthread_t;

ThreadId GetCurrentThreadId();
#endif

void setCurrentThreadName(const std::string& name);

////////////////////////////////////////////////////////////////////////////////

class UDT_API CTimer
{
public:
    CTimer();
    ~CTimer();

public:

    // Functionality:
    //    Sleep for "interval" CCs.
    // Parameters:
    //    0) [in] interval: CCs to sleep.
    // Returned value:
    //    None.

    void sleep(std::chrono::microseconds interval);

    // Functionality:
    //    Seelp until CC "nexttime".
    // Parameters:
    //    0) [in] nexttime: next time the caller is waken up.
    // Returned value:
    //    None.

    void sleepto(std::chrono::microseconds nexttime);

    // Functionality:
    //    Stop the sleep() or sleepto() methods.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    void interrupt();

    // Functionality:
    //    trigger the clock for a tick, for better granuality in no_busy_waiting timer.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    void tick();

public:

    // Functionality:
    //    Read the CPU clock cycle into x.
    // Parameters:
    //    0) [out] x: to record cpu clock cycles.
    // Returned value:
    //    None.

    static void rdtsc(uint64_t &x);

    // Functionality:
    //    return the CPU frequency.
    // Parameters:
    //    None.
    // Returned value:
    //    CPU frequency.

    static uint64_t getCPUFrequency();

    // Functionality:
    //    check the current time, 64bit, in microseconds.
    // Parameters:
    //    None.
    // Returned value:
    //    current time in microseconds.

    static std::chrono::microseconds getTime();

    // Functionality:
    //    trigger an event such as new connection, close, new data, etc. for "select" call.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    static void triggerEvent();

    // Functionality:
    //    wait for an event to br triggered by "triggerEvent".
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    static void waitForEvent();

    // Functionality:
    //    sleep for a short interval. exact sleep time does not matter
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    static void sleep();

private:
    uint64_t getTimeInMicroSec();

private:
    std::chrono::microseconds m_ullSchedTime;             // next schedulled time

                                         //#ifdef _WIN32
                                         //   OSVERSIONINFO m_winVersion;
                                         //#endif

    pthread_cond_t m_TickCond;
    pthread_mutex_t m_TickLock;

    static pthread_cond_t m_EventCond;
    static pthread_mutex_t m_EventLock;
    static int m_EventCondInit;

private:
    static uint64_t s_ullCPUFrequency;    // CPU frequency : clock cycles per microsecond
    static uint64_t readCPUFrequency();
    static bool m_bUseMicroSecond;       // No higher resolution timer available, use gettimeofday().
};

////////////////////////////////////////////////////////////////////////////////

// UDT Sequence Number 0 - (2^31 - 1)

// seqcmp: compare two seq#, considering the wraping
// seqlen: length from the 1st to the 2nd seq#, including both
// seqoff: offset from the 2nd to the 1st seq#
// incseq: increase the seq# by 1
// decseq: decrease the seq# by 1
// incseq: increase the seq# by a given offset

class CSeqNo
{
public:
    static int seqcmp(int32_t seq1, int32_t seq2)
    {
        return (abs(seq1 - seq2) < m_iSeqNoTH) ? (seq1 - seq2) : (seq2 - seq1);
    }

    static int seqlen(int32_t seq1, int32_t seq2)
    {
        return (seq1 <= seq2) ? (seq2 - seq1 + 1) : (seq2 - seq1 + m_iMaxSeqNo + 2);
    }

    static int seqoff(int32_t seq1, int32_t seq2)
    {
        if (abs(seq1 - seq2) < m_iSeqNoTH)
            return seq2 - seq1;

        if (seq1 < seq2)
            return seq2 - seq1 - m_iMaxSeqNo - 1;

        return seq2 - seq1 + m_iMaxSeqNo + 1;
    }

    static int32_t incseq(int32_t seq)
    {
        return (seq == m_iMaxSeqNo) ? 0 : seq + 1;
    }

    static int32_t decseq(int32_t seq)
    {
        return (seq == 0) ? m_iMaxSeqNo : seq - 1;
    }

    static int32_t incseq(int32_t seq, int32_t inc)
    {
        return (m_iMaxSeqNo - seq >= inc) ? seq + inc : seq - m_iMaxSeqNo + inc - 1;
    }

public:
    static const int32_t m_iSeqNoTH;             // threshold for comparing seq. no.
    static const int32_t m_iMaxSeqNo;            // maximum sequence number used in UDT
};

////////////////////////////////////////////////////////////////////////////////

// UDT ACK Sub-sequence Number: 0 - (2^31 - 1)

class CAckNo
{
public:
    static int32_t incack(int32_t ackno)
    {
        return (ackno == m_iMaxAckSeqNo) ? 0 : ackno + 1;
    }

public:
    static const int32_t m_iMaxAckSeqNo;         // maximum ACK sub-sequence number used in UDT
};

////////////////////////////////////////////////////////////////////////////////

// UDT Message Number: 0 - (2^29 - 1)

class CMsgNo
{
public:
    static int msgcmp(int32_t msgno1, int32_t msgno2)
    {
        return (abs(msgno1 - msgno2) < m_iMsgNoTH) ? (msgno1 - msgno2) : (msgno2 - msgno1);
    }

    static int msglen(int32_t msgno1, int32_t msgno2)
    {
        return (msgno1 <= msgno2) ? (msgno2 - msgno1 + 1) : (msgno2 - msgno1 + m_iMaxMsgNo + 2);
    }

    static int msgoff(int32_t msgno1, int32_t msgno2)
    {
        if (abs(msgno1 - msgno2) < m_iMsgNoTH)
            return msgno2 - msgno1;

        if (msgno1 < msgno2)
            return msgno2 - msgno1 - m_iMaxMsgNo - 1;

        return msgno2 - msgno1 + m_iMaxMsgNo + 1;
    }

    static int32_t incmsg(int32_t msgno)
    {
        return (msgno == m_iMaxMsgNo) ? 0 : msgno + 1;
    }

public:
    static const int32_t m_iMsgNoTH;             // threshold for comparing msg. no.
    static const int32_t m_iMaxMsgNo;            // maximum message number used in UDT
};

////////////////////////////////////////////////////////////////////////////////

struct CIPAddress
{
    static bool ipcmp(const sockaddr* addr1, const sockaddr* addr2);
    static void ntop(const sockaddr* addr, uint32_t ip[4], int ver = AF_INET);
    static void pton(detail::SocketAddress* addr, const uint32_t ip[4], int ver = AF_INET);
};

////////////////////////////////////////////////////////////////////////////////

struct CMD5
{
    static void compute(const char* input, unsigned char result[16]);
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Should be used to report OS-specific error.
 * E.g.,
 * <pre><code>
 * auto handle = socket(...);
 * if (handle < 0)
 *     return OsError();
 * </code></pre>
 */
class OsError:
    public UDT::Error
{
};

namespace OsErrorCode
{
    static constexpr UDT::Errno ok = 0;

#if defined(_WIN32)
    static constexpr UDT::Errno addressFamilyNotSupported = WSAEAFNOSUPPORT;
    static constexpr UDT::Errno protocolNotSupported = WSAEPROTONOSUPPORT;
    static constexpr UDT::Errno badDescriptor = WSAEBADF;
    static constexpr UDT::Errno connectionRefused = WSAECONNREFUSED;
    static constexpr UDT::Errno invalidData = WSAEINVAL;
    static constexpr UDT::Errno wouldBlock = WSAEWOULDBLOCK;
    static constexpr UDT::Errno already = WSAEALREADY;
    static constexpr UDT::Errno notConnected = WSAENOTCONN;
    static constexpr UDT::Errno isConnected = WSAEISCONN;
    static constexpr UDT::Errno noProtocolOption = WSAENOPROTOOPT;
    static constexpr UDT::Errno addressInUse = WSAEADDRINUSE;
    static constexpr UDT::Errno notSupported = WSAEOPNOTSUPP;
    static constexpr UDT::Errno timedOut = WSAETIMEDOUT;
    static constexpr UDT::Errno messageTooLarge = WSAEMSGSIZE;
    static constexpr UDT::Errno connectionReset = WSAECONNRESET;
    static constexpr UDT::Errno io = ERROR_GEN_FAILURE;
#else
    static constexpr UDT::Errno addressFamilyNotSupported = EAFNOSUPPORT;
    static constexpr UDT::Errno protocolNotSupported = EPROTONOSUPPORT;
    static constexpr UDT::Errno badDescriptor = EBADF;
    static constexpr UDT::Errno connectionRefused = ECONNREFUSED;
    static constexpr UDT::Errno invalidData = EINVAL;
    static constexpr UDT::Errno wouldBlock = EWOULDBLOCK;
    static constexpr UDT::Errno already = EALREADY;
    static constexpr UDT::Errno notConnected = ENOTCONN;
    static constexpr UDT::Errno isConnected = EISCONN;
    static constexpr UDT::Errno noProtocolOption = ENOPROTOOPT;
    static constexpr UDT::Errno addressInUse = EADDRINUSE;
    static constexpr UDT::Errno notSupported = EOPNOTSUPP;
    static constexpr UDT::Errno timedOut = ETIMEDOUT;
    static constexpr UDT::Errno messageTooLarge = EMSGSIZE;
    static constexpr UDT::Errno connectionReset = ECONNRESET;
    static constexpr UDT::Errno io = EIO;
#endif
}

#endif
