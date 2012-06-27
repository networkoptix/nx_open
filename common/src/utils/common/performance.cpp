#include "performance.h"
#include <QtGlobal>

#if 0

#ifdef Q_OS_WIN
#   define NOMINMAX
#   include <Windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
    qint64 fileTimeToNSec(const FILETIME &fileTime) {
        LARGE_INTEGER intTime;
        intTime.HighPart = fileTime.dwHighDateTime;
        intTime.LowPart = fileTime.dwLowDateTime;
            
        /* Convert from 100-nanoseconds to nanoseconds. */
        return intTime.QuadPart * 100;
    }

    qint64 estimateCpuFrequency() {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);

        DWORD64 startCycles = __rdtsc();
        
        LARGE_INTEGER stop;
        QueryPerformanceCounter(&stop);
        stop.QuadPart += freq.QuadPart / 10; /* Run for 0.1 sec. */

        LARGE_INTEGER current;
        do {
            QueryPerformanceCounter(&current);
        } while (current.QuadPart < stop.QuadPart);

        DWORD64 endCycles = __rdtsc();

        return (endCycles - startCycles) * 10;
    }

    Q_GLOBAL_STATIC_WITH_INITIALIZER(qint64, qn_estimatedCpuFrequency, { *x = estimateCpuFrequency(); });
#endif

} // anonymous namespace


qint64 QnPerformance::currentThreadTimeMSecs() {
    qint64 result = currentThreadTimeNSecs();
    if(result == -1)
        return -1;
    return result / 1000000;
}

qint64 QnPerformance::currentThreadTimeNSecs() {
#ifdef Q_OS_WIN
    FILETIME userTime, kernelTime, t0, t1;
    BOOL status = GetThreadTimes(GetCurrentThread(), &t0, &t1, &kernelTime, &userTime);
    if(!SUCCEEDED(status))
        return -1;
    return fileTimeToNSec(userTime) + fileTimeToNSec(kernelTime);
#else
    return -1;
#endif
}

qint64 QnPerformance::currentThreadCycles() {
#ifdef Q_OS_WIN
    ULONG64 time;
    BOOL status = QueryThreadCycleTime(GetCurrentThread(), &time);
    if(!SUCCEEDED(status))
        return -1;
    return time;
#else
    return -1;
#endif
}

qint64 QnPerformance::currentCpuFrequency() {
#ifdef Q_OS_WIN
    return *qn_estimatedCpuFrequency();
#else
    return -1;
#endif
}

#endif
