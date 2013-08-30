/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "socket.h"


AbstractDatagramSocket* SocketFactory::createDatagramSocket()
{
    return new UDPSocket();
}

AbstractStreamSocket* SocketFactory::createStreamSocket( bool /*sslRequired*/, SocketFactory::NatTraversalType /*natTraversalRequired*/ )
{
    return new TCPSocket();
}

AbstractStreamServerSocket* SocketFactory::createStreamServerSocket( bool /*sslRequired*/, SocketFactory::NatTraversalType /*natTraversalRequired*/ )
{
    return new TCPServerSocket();
}
