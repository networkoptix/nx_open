/*****************************************************************************
Copyright (c) 2001 - 2010, The Board of Trustees of the University of Illinois.
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
Yunhong Gu, last updated 07/25/2010
*****************************************************************************/


#ifndef _WIN32
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <time.h>
#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef LEGACY_WIN32
#include <wspiapi.h>
#endif
#endif

#include <cmath>
#include "md5.h"
#include "common.h"

#ifndef _WIN32
int pthread_cond_init_monotonic(pthread_cond_t* condition)
{
#if !defined(__APPLE__) && !defined(__ANDROID__)
    pthread_condattr_t attr;
    if (auto r = pthread_condattr_init(&attr))
        return r;

    if (auto r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC))
        return r;

    auto r = pthread_cond_init(condition, &attr);
    pthread_condattr_destroy(&attr);
    return r;
#else
    return pthread_cond_init(condition, NULL);
#endif
}

int pthread_cond_wait_monotonic_timeout(
    pthread_cond_t* condition, pthread_mutex_t* mutex, uint64_t timeoutMks)
{
    timespec timeout;
#ifdef __APPLE__
    timeout.tv_sec = (timeoutMks / 1000000);
    timeout.tv_nsec = (timeoutMks % 1000000) * 1000;

    return pthread_cond_timedwait_relative_np(condition, mutex, &timeout);
#else
    if (auto t = clock_gettime(CLOCK_MONOTONIC, &timeout))
        return t;

    timeout.tv_sec += (timeoutMks / 1000000);
    timeout.tv_nsec += (timeoutMks % 1000000) * 1000;
    if (timeout.tv_nsec >= 1000000000)
    {
        timeout.tv_sec += 1;
        timeout.tv_nsec -= 1000000000;
    }

    return pthread_cond_timedwait(condition, mutex, &timeout);
#endif
}

int pthread_cond_wait_monotonic_timepoint(
    pthread_cond_t* condition, pthread_mutex_t* mutex, uint64_t timeMks)
{
#ifdef __APPLE__
    const auto now = CTimer::getTime();
    if (now > timeMks)
        return ETIMEDOUT;

    return pthread_cond_wait_monotonic_timeout(condition, mutex, timeMks - now);
#else
    timespec locktime;
    locktime.tv_sec = timeMks / 1000000;
    locktime.tv_nsec = (timeMks % 1000000) * 1000;

    return pthread_cond_timedwait(condition, mutex, &locktime);
#endif
}
#endif

#ifndef _WIN32
ThreadId GetCurrentThreadId()
{
    return pthread_self();
}
#endif

bool CTimer::m_bUseMicroSecond = false;
uint64_t CTimer::s_ullCPUFrequency = CTimer::readCPUFrequency();
#ifndef _WIN32
pthread_mutex_t CTimer::m_EventLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t CTimer::m_EventCond;
int CTimer::m_EventCondInit = pthread_cond_init_monotonic(&CTimer::m_EventCond);
#else
pthread_mutex_t CTimer::m_EventLock = CreateMutex(NULL, false, NULL);
pthread_cond_t CTimer::m_EventCond = CreateEvent(NULL, false, false, NULL);
#endif

CTimer::CTimer()
    :
    m_ullSchedTime(),
    m_TickCond(),
    m_TickLock()
{
#ifndef _WIN32
    pthread_mutex_init(&m_TickLock, NULL);
    pthread_cond_init_monotonic(&m_TickCond);
#else
    m_TickLock = CreateMutex(NULL, false, NULL);
    m_TickCond = CreateEvent(NULL, false, false, NULL);
#endif
}

CTimer::~CTimer()
{
#ifndef _WIN32
    pthread_mutex_destroy(&m_TickLock);
    pthread_cond_destroy(&m_TickCond);
#else
    CloseHandle(m_TickLock);
    CloseHandle(m_TickCond);
#endif
}

void CTimer::rdtsc(uint64_t &x)
{
    if (m_bUseMicroSecond)
    {
        x = getTime();
        return;
    }

#ifdef IA32
    uint32_t lval, hval;
    //asm volatile ("push %eax; push %ebx; push %ecx; push %edx");
    //asm volatile ("xor %eax, %eax; cpuid");
    asm volatile ("rdtsc" : "=a" (lval), "=d" (hval));
    //asm volatile ("pop %edx; pop %ecx; pop %ebx; pop %eax");
    x = hval;
    x = (x << 32) | lval;
#elif defined(IA64)
    asm("mov %0=ar.itc" : "=r"(x) :: "memory");
#elif defined(AMD64)
    uint32_t lval, hval;
    asm("rdtsc" : "=a" (lval), "=d" (hval));
    x = hval;
    x = (x << 32) | lval;
#elif defined(_WIN32)
    //HANDLE hCurThread = ::GetCurrentThread();
    //DWORD_PTR dwOldMask = ::SetThreadAffinityMask(hCurThread, 1);
    BOOL ret = QueryPerformanceCounter((LARGE_INTEGER *)&x);
    //SetThreadAffinityMask(hCurThread, dwOldMask);
    if (!ret)
        x = getTime() * s_ullCPUFrequency;
#elif defined(__APPLE__)
    x = mach_absolute_time();
#else
    // use system call to read time clock for other archs
    x = getTime();
#endif
}

uint64_t CTimer::readCPUFrequency()
{
    uint64_t frequency = 1;  // 1 tick per microsecond.

#if defined(IA32) || defined(IA64) || defined(AMD64)
    uint64_t t1, t2;

    rdtsc(t1);
    timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000;
    nanosleep(&ts, NULL);
    rdtsc(t2);

    // CPU clocks per microsecond
    frequency = (t2 - t1) / 100000;
#elif defined(_WIN32)
    int64_t ccf;
    if (QueryPerformanceFrequency((LARGE_INTEGER *)&ccf))
        frequency = ccf / 1000000;
#elif defined(__APPLE__)
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    frequency = info.denom * 1000ULL / info.numer;
#endif

    // Fall back to microsecond if the resolution is not high enough.
    if (frequency < 10)
    {
        frequency = 1;
        m_bUseMicroSecond = true;
    }
    return frequency;
}

uint64_t CTimer::getCPUFrequency()
{
    return s_ullCPUFrequency;
}

void CTimer::sleep(uint64_t interval)
{
    uint64_t t;
    rdtsc(t);

    // sleep next "interval" time
    sleepto(t + interval);
}

void CTimer::sleepto(uint64_t nexttime)
{
    // Use class member such that the method can be interrupted by others
    m_ullSchedTime = nexttime;

    uint64_t t;
    rdtsc(t);

    while (t < m_ullSchedTime)
    {
#ifndef NO_BUSY_WAITING
#ifdef IA32
        __asm__ volatile ("pause; rep; nop; nop; nop; nop; nop;");
#elif IA64
        __asm__ volatile ("nop 0; nop 0; nop 0; nop 0; nop 0;");
#elif AMD64
        __asm__ volatile ("nop; nop; nop; nop; nop;");
#endif
#else
#ifndef _WIN32
        pthread_mutex_lock(&m_TickLock);
        pthread_cond_wait_monotonic_timeout(&m_TickCond, &m_TickLock, 10 * 1000); // 10ms
        pthread_mutex_unlock(&m_TickLock);
#else
        WaitForSingleObject(m_TickCond, 1);
#endif
#endif

        rdtsc(t);
    }
}

void CTimer::interrupt()
{
    // schedule the sleepto time to the current CCs, so that it will stop
    rdtsc(m_ullSchedTime);
    tick();
}

void CTimer::tick()
{
#ifndef _WIN32
    pthread_cond_signal(&m_TickCond);
#else
    SetEvent(m_TickCond);
#endif
}

uint64_t CTimer::getTime()
{
    //For Cygwin and other systems without microsecond level resolution, uncomment the following three lines
    //uint64_t x;
    //rdtsc(x);
    //return x / s_ullCPUFrequency;
    //Specific fix may be necessary if rdtsc is not available either.

#ifndef _WIN32
#ifdef __APPLE__
    clock_serv_t clock;
    host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &clock);

    mach_timespec_t t;
    clock_get_time(clock, &t);

    mach_port_deallocate(mach_task_self(), clock);
#else
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
#endif

    return (uint64_t)t.tv_sec * (uint64_t)1000000 + (uint64_t)(t.tv_nsec / 1000);
#else
    LARGE_INTEGER ccf;
    HANDLE hCurThread = ::GetCurrentThread();
    //DWORD_PTR dwOldMask = 0;
    //if (m_winVersion.dwMajorVersion < 6)
    //  dwOldMask = ::SetThreadAffinityMask(hCurThread, 1);
    if (QueryPerformanceFrequency(&ccf))
    {
        LARGE_INTEGER cc;
        if (QueryPerformanceCounter(&cc))
        {
            //if (m_winVersion.dwMajorVersion < 6)
            //    SetThreadAffinityMask(hCurThread, dwOldMask);
            return (cc.QuadPart * 1000000ULL / ccf.QuadPart);
        }
    }

    //if (m_winVersion.dwMajorVersion < 6)
    //    SetThreadAffinityMask(hCurThread, dwOldMask);
    return GetTickCount() * 1000ULL;
#endif
}

void CTimer::triggerEvent()
{
#ifndef _WIN32
    pthread_cond_signal(&m_EventCond);
#else
    SetEvent(m_EventCond);
#endif
}

void CTimer::waitForEvent()
{
#ifndef _WIN32
    pthread_mutex_lock(&m_EventLock);
    pthread_cond_wait_monotonic_timeout(&m_EventCond, &m_EventLock, 10 * 1000); // 10ms
    pthread_mutex_unlock(&m_EventLock);
#else
    WaitForSingleObject(m_EventCond, 1);
#endif
}

void CTimer::sleep()
{
#ifndef _WIN32
    usleep(10);
#else
    Sleep(1);
#endif
}

////////////////////////////////////////////////////////////////////////////////

bool CIPAddress::ipcmp(
    const sockaddr* addr1,
    const sockaddr* addr2)
{
    if (addr1->sa_family != addr2->sa_family)
        return false;

    if (addr1->sa_family == AF_INET)
    {
        sockaddr_in* a1 = (sockaddr_in*)addr1;
        sockaddr_in* a2 = (sockaddr_in*)addr2;

        if ((a1->sin_port == a2->sin_port) && (a1->sin_addr.s_addr == a2->sin_addr.s_addr))
            return true;
    }
    else if (addr1->sa_family == AF_INET6)
    {
        sockaddr_in6* a1 = (sockaddr_in6*)addr1;
        sockaddr_in6* a2 = (sockaddr_in6*)addr2;

        if (a1->sin6_port == a2->sin6_port)
        {
            for (int i = 0; i < 16; ++i)
                if (*((char*)&(a1->sin6_addr) + i) != *((char*)&(a2->sin6_addr) + i))
                    return false;

            return true;
        }
    }

    return false;
}

void CIPAddress::ntop(const sockaddr* addr, uint32_t ip[4], int ver)
{
    if (AF_INET == ver)
    {
        sockaddr_in* a = (sockaddr_in*)addr;
        ip[0] = a->sin_addr.s_addr;
    }
    else
    {
        sockaddr_in6* a = (sockaddr_in6*)addr;
        ip[3] = (a->sin6_addr.s6_addr[15] << 24) + (a->sin6_addr.s6_addr[14] << 16) + (a->sin6_addr.s6_addr[13] << 8) + a->sin6_addr.s6_addr[12];
        ip[2] = (a->sin6_addr.s6_addr[11] << 24) + (a->sin6_addr.s6_addr[10] << 16) + (a->sin6_addr.s6_addr[9] << 8) + a->sin6_addr.s6_addr[8];
        ip[1] = (a->sin6_addr.s6_addr[7] << 24) + (a->sin6_addr.s6_addr[6] << 16) + (a->sin6_addr.s6_addr[5] << 8) + a->sin6_addr.s6_addr[4];
        ip[0] = (a->sin6_addr.s6_addr[3] << 24) + (a->sin6_addr.s6_addr[2] << 16) + (a->sin6_addr.s6_addr[1] << 8) + a->sin6_addr.s6_addr[0];
    }
}

void CIPAddress::pton(detail::SocketAddress* addr, const uint32_t ip[4], int ver)
{
    if (AF_INET == ver)
    {
        sockaddr_in* a = &addr->v4();
        a->sin_addr.s_addr = ip[0];
    }
    else
    {
        sockaddr_in6* a = &addr->v6();
        for (int i = 0; i < 4; ++i)
        {
            a->sin6_addr.s6_addr[i * 4] = ip[i] & 0xFF;
            a->sin6_addr.s6_addr[i * 4 + 1] = (unsigned char)((ip[i] & 0xFF00) >> 8);
            a->sin6_addr.s6_addr[i * 4 + 2] = (unsigned char)((ip[i] & 0xFF0000) >> 16);
            a->sin6_addr.s6_addr[i * 4 + 3] = (unsigned char)((ip[i] & 0xFF000000) >> 24);
        }
    }
}

//
void CMD5::compute(const char* input, unsigned char result[16])
{
    md5_state_t state;

    md5_init(&state);
    md5_append(&state, (const md5_byte_t *)input, strlen(input));
    md5_finish(&state, result);
}
