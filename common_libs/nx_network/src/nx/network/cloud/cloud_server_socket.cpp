#include "cloud_server_socket.h"

namespace nx {
namespace network {
namespace cloud {

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

void CloudServerSocket::acceptAsync(
    std::function<void(
        SystemError::ErrorCode,
        AbstractStreamSocket* )> /*handler*/ )
{
    //TODO #ak
}

} // namespace cloud
} // namespace network
} // namespace nx
