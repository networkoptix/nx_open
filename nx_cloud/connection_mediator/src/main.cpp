/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "mediator_service.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <signal.h>
#endif

static MediatorProcess* serviceInstance = NULL;

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

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleCtrlHandler(stopServer_WIN, TRUE);
#else
    signal(SIGINT, stopServer);
    signal(SIGTERM, stopServer);
#endif

    MediatorProcess service(argc, argv);
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
