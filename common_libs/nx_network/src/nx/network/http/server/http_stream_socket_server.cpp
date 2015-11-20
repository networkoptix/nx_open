/**********************************************************
* Sep 7, 2015
* akolesnikov
***********************************************************/

#include "http_stream_socket_server.h"


namespace nx_http
{

HttpStreamSocketServer::HttpStreamSocketServer(
    nx_http::AbstractAuthenticationManager* const authenticationManager,
    nx_http::MessageDispatcher* const httpMessageDispatcher,
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired)
:
    base_type(sslRequired, natTraversalRequired),
    m_authenticationManager(authenticationManager),
    m_httpMessageDispatcher(httpMessageDispatcher)
{
}

std::shared_ptr<HttpServerConnection> HttpStreamSocketServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    return std::make_shared<HttpServerConnection>(
        this,
        std::move(_socket),
        m_authenticationManager,
        m_httpMessageDispatcher);
}

}
