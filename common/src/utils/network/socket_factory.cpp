/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "system_socket.h"


AbstractDatagramSocket* SocketFactory::createDatagramSocket()
{
    return new UDPSocket();
}

AbstractStreamSocket* SocketFactory::createStreamSocket( bool /*sslRequired*/, SocketFactory::NatTraversalType /*natTraversalRequired*/ )
{
    return new TCPSocket();
}

AbstractStreamServerSocket* SocketFactory::createStreamServerSocket( bool sslRequired, SocketFactory::NatTraversalType /*natTraversalRequired*/ )
{
    if (sslRequired)
        return new TCPSslServerSocket();
    else
        return new TCPServerSocket();
}
