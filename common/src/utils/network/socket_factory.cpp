/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "mixed_tcp_udt_server_socket.h"
#include "system_socket.h"
#include "ssl_socket.h"


std::unique_ptr< AbstractDatagramSocket > SocketFactory::createDatagramSocket()
{
    return std::unique_ptr< AbstractDatagramSocket >( new UDPSocket() );
}

std::unique_ptr< AbstractStreamSocket > SocketFactory::createStreamSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    AbstractStreamSocket* result = nullptr;
    switch( natTraversalRequired )
    {
        case nttAuto:
        case nttEnabled:
            //TODO #ak nat_traversal MUST be enabled explicitly by instanciating some singletone
            result = new TCPSocket( true ); //new HybridStreamSocket();  //that's where hole punching kicks in
            break;
        //case nttEnabled:
        //    result = new UdtStreamSocket();   //TODO #ak does it make sense to use UdtStreamSocket only? - yes, but with different SocketFactory::NatTraversalType value
        //    break;
        case nttDisabled:
            result = new TCPSocket( false );
            break;
    }

    if( !result )
        return nullptr;

#ifdef ENABLE_SSL
    if (sslRequired)
        result = new QnSSLSocket(result, false);
#endif
    
    return std::unique_ptr< AbstractStreamSocket >( result );
}

std::unique_ptr< AbstractStreamServerSocket > SocketFactory::createStreamServerSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    AbstractStreamServerSocket* serverSocket = nullptr;
    switch( natTraversalRequired )
    {
        case nttAuto:
        case nttEnabled:
            //TODO #ak cloud_acceptor
            serverSocket = new MixedTcpUdtServerSocket();
            //serverSocket = new TCPServerSocket();
            break;
        //case nttEnabled:
        //    serverSocket = new UdtStreamServerSocket();
        //    break;
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

    return std::unique_ptr< AbstractStreamServerSocket >( serverSocket );
}
