#include "multi_endpoint_acceptor.h"

namespace nx::network::http::server {

MultiEndpointAcceptor::MultiEndpointAcceptor(
    server::AbstractAuthenticationManager* authenticationManager,
    AbstractMessageDispatcher* httpMessageDispatcher)
    :
    m_authenticationManager(authenticationManager),
    m_httpMessageDispatcher(httpMessageDispatcher)
{
}

network::server::Statistics MultiEndpointAcceptor::statistics() const
{
    return m_multiAddressHttpServer->statistics();
}

void MultiEndpointAcceptor::pleaseStopSync()
{
    if (m_multiAddressHttpServer)
        m_multiAddressHttpServer->pleaseStopSync();
}

bool MultiEndpointAcceptor::bind(
    const std::vector<SocketAddress>& endpoints,
    const std::vector<SocketAddress>& sslEndpoints)
{
    if (endpoints.empty() && sslEndpoints.empty())
    {
        SystemError::setLastErrorCode(SystemError::invalidData);
        return false;
    }

    if (endpoints.empty())
    {
        m_multiAddressHttpServer = startHttpsServer(sslEndpoints);
    }
    else if (sslEndpoints.empty())
    {
        m_multiAddressHttpServer = startHttpServer(endpoints);
    }
    else
    {
        auto regularServer = startHttpServer(endpoints);
        if (!regularServer)
            return false;

        auto securedServer = startHttpsServer(sslEndpoints);
        if (!securedServer)
            return false;

        m_multiAddressHttpServer = network::server::catMultiAddressServers(
            std::move(regularServer),
            std::move(securedServer));
    }

    return m_multiAddressHttpServer != nullptr;
}

bool MultiEndpointAcceptor::listen(int backlogSize)
{
    if (!m_multiAddressHttpServer)
    {
        SystemError::setLastErrorCode(SystemError::badDescriptor);
        return false;
    }

    return m_multiAddressHttpServer->listen(backlogSize);
}

std::vector<SocketAddress> MultiEndpointAcceptor::endpoints() const
{
    return m_endpoints;
}

std::vector<SocketAddress> MultiEndpointAcceptor::sslEndpoints() const
{
    return m_sslEndpoints;
}

std::unique_ptr<MultiEndpointAcceptor::MultiHttpServer>
    MultiEndpointAcceptor::startHttpServer(
        const std::vector<network::SocketAddress>& endpoints)
{
    auto server = startServer(endpoints, false);
    if (server)
        m_endpoints = server->endpoints();
    return server;
}

std::unique_ptr<MultiEndpointAcceptor::MultiHttpServer>
    MultiEndpointAcceptor::startHttpsServer(
        const std::vector<network::SocketAddress>& endpoints)
{
    auto server = startServer(endpoints, true);
    if (server)
        m_sslEndpoints = server->endpoints();
    return server;
}

std::unique_ptr<MultiEndpointAcceptor::MultiHttpServer>
    MultiEndpointAcceptor::startServer(
        const std::vector<network::SocketAddress>& endpoints,
        bool sslMode)
{
    auto multiAddressHttpServer =
        std::make_unique<MultiHttpServer>(
            m_authenticationManager,
            m_httpMessageDispatcher,
            sslMode,
            nx::network::NatTraversalSupport::disabled);

    if (!multiAddressHttpServer->bind(endpoints))
        return nullptr;

    return multiAddressHttpServer;
}

} // namespace nx::network::http::server
