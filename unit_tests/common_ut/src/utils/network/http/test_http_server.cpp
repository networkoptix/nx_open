/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "test_http_server.h"


TestHttpServer::TestHttpServer()
{
    m_httpServerManagers.setDispatcher( &m_httpMessageDispatcher );
    m_httpServer.reset(
        new nx_http::HttpStreamSocketServer(
            false,
            SocketFactory::nttDisabled ) );
}

TestHttpServer::~TestHttpServer()
{
}

bool TestHttpServer::bindAndListen()
{
    return m_httpServer->bind( SocketAddress( HostAddress::localhost, 0 ) )
        && m_httpServer->listen();
}

SocketAddress TestHttpServer::serverAddress() const
{
    return m_httpServer->address();
}
