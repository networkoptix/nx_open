#include "cloud_server_socket.h"

namespace nx {
namespace cc {

bool CloudServerSocket::bind( const SocketAddress& /*endpoint*/ )
{
    //TODO #ak
    return false;
}

bool CloudServerSocket::listen( int /*queueLen*/ )
{
    //TODO #ak
    return false;
}

AbstractStreamSocket* CloudServerSocket::accept()
{
    //TODO #ak
    return nullptr;
}

void CloudServerSocket::pleaseStop( std::function< void() > /*handler*/ )
{
    //TODO #ak
}

void CloudServerSocket::acceptAsyncImpl(
    std::function<void(
        SystemError::ErrorCode,
        AbstractStreamSocket* )>&& /*handler*/ )
{
    //TODO #ak
}

} // namespace cc
} // namespace nx
