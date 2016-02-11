/**********************************************************
* Nov 25, 2015
* akolesnikov
***********************************************************/

#include "thread_util.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#if defined(__linux__) && !defined(Q_OS_ANDROID)
#   include <sys/types.h>
#   include <sys/syscall.h>
#   include <unistd.h>
static pid_t gettid(void) { return syscall(__NR_gettid); }
#elif __APPLE__
#include <pthread.h>
#endif


uintptr_t currentThreadSystemId()
{
#if __linux__
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
#endif
}
