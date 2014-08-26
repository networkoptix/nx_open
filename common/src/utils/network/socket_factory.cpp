/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "mixed_tcp_udt_server_socket.h"
#include "system_socket.h"
#include "ssl_socket.h"


AbstractDatagramSocket* SocketFactory::createDatagramSocket()
{
    return new UDPSocket();
}

AbstractStreamSocket* SocketFactory::createStreamSocket( bool sslRequired, SocketFactory::NatTraversalType natTraversalRequired )
{
    AbstractStreamSocket* result = new TCPSocket();
    switch( natTraversalRequired )
    {
        case nttAuto:
            result = new TCPSocket(); //TODO #ak
            break;
        case nttEnabled:
            result = new UdtStreamSocket();
            break;
        case nttDisabled:
            result = new TCPSocket();
            break;
    }

    assert( result );
    if( !result )
        return nullptr;

#ifdef ENABLE_SSL
    if (sslRequired)
        result = new QnSSLSocket(result, false);
#endif
    
    return result;
}

AbstractStreamServerSocket* SocketFactory::createStreamServerSocket( bool sslRequired, SocketFactory::NatTraversalType natTraversalRequired )
{
    AbstractStreamServerSocket* serverSocket = nullptr;
    switch( natTraversalRequired )
    {
        case nttAuto:
            serverSocket = new MixedTcpUdtServerSocket();
            break;
        case nttEnabled:
            serverSocket = new UdtStreamServerSocket();
            break;
        case nttDisabled:
            serverSocket = new TCPServerSocket();
            break;
    }

    assert(serverSocket);
    if( !serverSocket )
        return nullptr;

#ifdef ENABLE_SSL
    if( sslRequired )
        serverSocket = new SSLServerSocket(serverSocket, true);
#endif // ENABLE_SSL
    return serverSocket;
}
