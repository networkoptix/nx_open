#include <nx/network/socket_global.h>
#include <nx/utils/crash_dump/systemexcept.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <signal.h>
#endif

#include "relay_service.h"

namespace nx {
namespace cloud {
namespace relay {

static nx::utils::Service* serviceInstance = nullptr;

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

int trafficRelayMain(int argc, char* argv[])
{
    #if defined(_WIN32)
        win32_exception::installGlobalUnhandledExceptionHandler();

        SetConsoleCtrlHandler(stopServer_WIN, TRUE);
    #else
        signal(SIGINT, stopServer);
        signal(SIGTERM, stopServer);
    #endif

    nx::cloud::relay::RelayService service(argc, argv);
    serviceInstance = &service;
    const int result = service.exec();

    #if defined(_WIN32)
        SetConsoleCtrlHandler(stopServer_WIN, FALSE);
    #else
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
    #endif

    serviceInstance = nullptr;
    return result;
}

} // namespace relay
} // namespace cloud
} // namespace nx
