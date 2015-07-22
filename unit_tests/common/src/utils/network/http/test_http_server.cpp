/**********************************************************
* 9 jan 2015
* a.kolesnikov
***********************************************************/

#include "test_http_server.h"


TestHttpServer::TestHttpServer()
:
    m_httpServer(
        new nx_http::HttpStreamSocketServer(
            false,
            SocketFactory::nttDisabled ) )
{
    //let server choose any port available
    if( !m_httpServer->bind( SocketAddress( HostAddress::localhost, 0 ) ) ||
        !m_httpServer->listen() )
    {
        assert( false );
    }

    //m_httpMessageDispatcher.registerRequestProcessor(
    //    RegisterSystemHttpHandler::HANDLER_PATH,
    //    [&registerHttpHandler](HttpServerConnection* connection, nx_http::Message&& message) -> bool {
    //        return registerHttpHandler.processRequest( connection, std::move(message) );
    //    }
    //);
}

TestHttpServer::~TestHttpServer()
{
}

SocketAddress TestHttpServer::serverAddress() const
{
    return m_httpServer->address();
}
