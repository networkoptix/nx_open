/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "vms_gateway_process_public.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <signal.h>
#endif

#include <nx/utils/crash_dump/systemexcept.h>

static nx::cloud::gateway::VmsGatewayProcessPublic* serviceInstance = NULL;

void stopServer( int /*signal*/ )
{
    if( serviceInstance )
        serviceInstance->pleaseStop();
}

#ifdef _WIN32
BOOL WINAPI stopServer_WIN(DWORD dwCtrlType)
{
    //NOTE this handler is called by a different thread (not main)
    stopServer(dwCtrlType);
    return true;
}
#endif

NX_VMS_GATEWAY_API int libVmsGatewayMain(int argc, char* argv[])
{
#ifdef _WIN32
    win32_exception::installGlobalUnhandledExceptionHandler();

    SetConsoleCtrlHandler(stopServer_WIN, TRUE);
#else
    signal(SIGINT, stopServer);
    signal(SIGTERM, stopServer);
#endif

    nx::cloud::gateway::VmsGatewayProcessPublic service(argc, argv);
    serviceInstance = &service;
    const int result = service.exec();

#ifdef _WIN32
    SetConsoleCtrlHandler(stopServer_WIN, FALSE);
#else
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
#endif

    serviceInstance = NULL;
    return result;
}
