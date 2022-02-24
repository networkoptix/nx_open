// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "service_main.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#endif

#include <nx/utils/crash_dump/systemexcept.h>

namespace nx::utils::detail {

static Service* serviceInstance = nullptr;

void stopServer(int /*signal*/)
{
    if (serviceInstance)
        serviceInstance->pleaseStop();
}

#if defined(_WIN32)
BOOL WINAPI stopServer_WIN(DWORD dwCtrlType)
{
    //NOTE this handler is called by a different thread (not main)
    stopServer(dwCtrlType);
    return true;
}
#endif

void installProcessHandlers()
{
#if defined(_WIN32)
    win32_exception::installGlobalUnhandledExceptionHandler();

    SetConsoleCtrlHandler(stopServer_WIN, TRUE);
#else
    signal(SIGINT, stopServer);
    signal(SIGTERM, stopServer);
#endif
}

void uninstallProcessHandlers()
{
#if defined(_WIN32)
    SetConsoleCtrlHandler(stopServer_WIN, FALSE);
#else
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
#endif
}

void registerServiceInstance(Service* service)
{
    serviceInstance = service;
}

} // namespace nx::utils::detail
