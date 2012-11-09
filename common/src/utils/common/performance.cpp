#include "performance.h"

#include <QtCore/QtGlobal>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtCore/QLibrary>

#include "warnings.h"

#if defined(Q_OS_WIN)
#   define NOMINMAX
#   include <Windows.h>
#endif


#ifdef Q_OS_WIN
namespace {
    BOOL WINAPI queryThreadCycleTimeDefault(HANDLE , PULONG64) {
        return FALSE;
    }

    class QnPerformanceFunctions {
        typedef BOOL (WINAPI *PtrQueryThreadCycleTime)(HANDLE ThreadHandle, PULONG64 CycleTime);

    public:
        QnPerformanceFunctions() {
            QLibrary kernel32(QLatin1String("Kernel32"));    
            PtrQueryThreadCycleTime result = (PtrQueryThreadCycleTime) kernel32.resolve("QueryThreadCycleTime");
            if (result)
                queryThreadCycleTime = result;
            else
                queryThreadCycleTime = &queryThreadCycleTimeDefault;
        }

        PtrQueryThreadCycleTime queryThreadCycleTime;
    };

    Q_GLOBAL_STATIC(QnPerformanceFunctions, qn_performanceFunctions_instance);

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

    Q_GLOBAL_STATIC_WITH_ARGS(qint64, qn_estimatedCpuFrequency, (estimateCpuFrequency()));

} // anonymous namespace
#endif // Q_OS_WIN


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
    BOOL status = qn_performanceFunctions_instance()->queryThreadCycleTime(GetCurrentThread(), &time);
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