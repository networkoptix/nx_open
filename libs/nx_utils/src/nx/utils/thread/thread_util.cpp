#include "thread_util.h"
#include <QtCore/QtGlobal>

#ifdef _WIN32
#include <Windows.h>
#endif
#if defined(__linux__)
#   include <sys/types.h>
#   include <sys/syscall.h>
#   include <unistd.h>
#ifndef Q_OS_ANDROID
static pid_t gettid(void) { return syscall(__NR_gettid); }
#endif
#elif __APPLE__
#include <pthread.h>
#endif


uintptr_t currentThreadSystemId()
{
#if __linux__ //&& !defined(Q_OS_ANDROID)
    /* This one is purely for debugging purposes.
    * QThread::currentThreadId is implemented via pthread_self,
    * which is not an identifier you see in GDB. */
    return gettid();
#elif _WIN32
    return GetCurrentThreadId();
#elif __APPLE__
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    return tid;
#elif defined(Q_OS_ANDROID)
    return 0; // This function used in mutex analizer so far. Doesnt inmplemented yet.
#endif
}
