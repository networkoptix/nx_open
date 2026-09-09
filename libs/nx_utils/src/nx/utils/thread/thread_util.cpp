// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thread_util.h"

#include <QtCore/QString>

#include <nx/utils/log/log.h>

#if _WIN32
    #include <Windows.h>
#else
    #include <pthread.h>
#endif

#if defined(__linux__) || defined(__EMSCRIPTEN__)
    #include <sys/types.h>
    #include <sys/syscall.h>
    #include <unistd.h>
#endif


uintptr_t currentThreadSystemId()
{
#if defined(__linux__) || defined(__EMSCRIPTEN__)
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
    using SetThreadDescriptionFunction = std::add_pointer_t<HRESULT WINAPI(HANDLE, PCWSTR)>;
    const auto resolveSetThreadDescription =
        [](const char* moduleName) -> SetThreadDescriptionFunction
    {
        const HMODULE handle = GetModuleHandleA(moduleName);
        if (handle == NULL)
            return nullptr;

        return reinterpret_cast<SetThreadDescriptionFunction>(
            reinterpret_cast<void*>(GetProcAddress(handle, "SetThreadDescription")));
    };

    auto setThreadDescription = resolveSetThreadDescription("kernel32.dll");
    if (!setThreadDescription)
    {
        // Windows 10 1607 and Windows Server 2016 export this function only from KernelBase.dll.
        setThreadDescription = resolveSetThreadDescription("KernelBase.dll");
    }

    if (setThreadDescription)
    {
        const HRESULT result = setThreadDescription(
            GetCurrentThread(), qUtf16Printable(QString::fromStdString(name)));
        if (FAILED(result))
            NX_DEBUG(NX_SCOPE_TAG, "SetThreadDescription failed with HRESULT %1", result);
    }
    #elif defined(__APPLE__)
        pthread_setname_np(name.c_str());
    #else
        pthread_setname_np(pthread_self(), name.c_str());
    #endif
}

} // namespace nx::utils
