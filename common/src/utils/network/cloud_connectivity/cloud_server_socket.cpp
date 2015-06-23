/**********************************************************
* 22 jun 2015
* a.kolesnikov@networkoptix.com
***********************************************************/

#include "cloud_server_socket.h"


bool CloudServerSocket::listen( int queueLen )
{
    //TODO #ak
    return false;
}

AbstractStreamSocket* CloudServerSocket::accept()
{
    //TODO #ak
    return nullptr;
}

void CloudServerSocket::cancelAsyncIO( bool waitForRunningHandlerCompletion = true )
{
    //TODO #ak
}

bool CloudServerSocket::acceptAsyncImpl(
    std::function<void(
        SystemError::ErrorCode,
        AbstractStreamSocket* )>&& handler )
{
    //TODO #ak
    return false;
}
