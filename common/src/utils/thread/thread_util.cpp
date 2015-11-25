/**********************************************************
* Nov 25, 2015
* akolesnikov
***********************************************************/

#include "thread_util.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#   include <sys/types.h>
#   include <linux/unistd.h>
static pid_t gettid(void) { return syscall(__NR_gettid); }
#endif


uintptr_t currentThreadSystemId()
{
#if defined(Q_OS_LINUX)
    /* This one is purely for debugging purposes.
    * QThread::currentThreadId is implemented via pthread_self,
    * which is not an identifier you see in GDB. */
    return gettid();
#else
    return GetCurrentThreadId();
#endif
}
