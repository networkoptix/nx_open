#include "http_server.h"

#include "../settings.h"

namespace nx::cloud::storage::service {

HttpServer::HttpServer(const Settings& settings):
    m_settings(settings),
    m_multiAddressServer(
        &m_authenticationDispatcher,
        &m_messageDispatcher)
{
    registerApiHandlers();
}

network::http::server::MultiEndpointAcceptor&
    HttpServer::server()
{
    return m_multiAddressServer;
}

network::http::server::rest::MessageDispatcher&
    HttpServer::messageDispatcher()
{
    return m_messageDispatcher;
}

bool HttpServer::bind()
{
    return m_multiAddressServer.bind(
        m_settings.http().endpoints,
        m_settings.http().sslEndpoints);
}

bool HttpServer::listen()
{
    return m_multiAddressServer.listen(
        m_settings.http().tcpBacklogSize);
}

void HttpServer::stop()
{
    m_multiAddressServer.pleaseStopSync();
}

std::vector<network::SocketAddress> HttpServer::httpEndpoints() const
{
    return m_multiAddressServer.endpoints();
}

std::vector<network::SocketAddress> HttpServer::httpsEndpoints() const
{
    return m_multiAddressServer.sslEndpoints();
}

void HttpServer::registerApiHandlers()
{
    // TODO
}

} // namespace nx::cloud::storage::service
