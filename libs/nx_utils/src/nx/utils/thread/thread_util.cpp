// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thread_util.h"

#include <QtCore/QtGlobal>

#if _WIN32
    #include <Windows.h>
    #include <codecvt>
#else
    #include <pthread.h>
#endif

#if defined(__linux__)
    #include <sys/types.h>
    #include <sys/syscall.h>
    #include <unistd.h>
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
#else
    #error "Not implemented"
    return 0;
#endif
}

namespace nx::utils {

void setCurrentThreadName(std::string name)
{
    #if defined(_WIN32)
    using SetThreadDescription = std::add_pointer_t<HRESULT WINAPI(HANDLE, PCWSTR)>;
    if (const auto setThreadDescription = reinterpret_cast<SetThreadDescription>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetThreadDescription")))
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        setThreadDescription(GetCurrentThread(), converter.from_bytes(name).c_str());
    }
    #elif defined(__APPLE__)
        pthread_setname_np(name.c_str());
    #else
        pthread_setname_np(pthread_self(), name.c_str());
    #endif
}

} // namespace nx::utils
