/**********************************************************
* 18 sep 2013
* a.kolesnikov
***********************************************************/

#if 0

#include "stream_socket_server.h"


StreamSocketServer::StreamSocketServer( AbstractStreamServerSocket::AsyncAcceptHandler* newConnectionHandler )
:
    m_newConnectionHandler( newConnectionHandler )
{
}

bool StreamSocketServer::bind( const SocketAddress& addrToListen )
{
    //TODO/IMPL
    return false;
}

bool StreamSocketServer::listen()
{
    //TODO/IMPL
    return false;
}

void StreamSocketServer::newConnectionAccepted(
    AbstractStreamServerSocket* serverSocket,
    AbstractStreamSocket* acceptedConnection )
{
    //TODO/IMPL
    m_newConnectionHandler->newConnectionAccepted( serverSocket, acceptedConnection );
}

#endif
