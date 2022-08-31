// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multi_endpoint_acceptor.h"

#include <nx/network/ssl/context.h>

namespace nx::network::http::server {

MultiEndpointAcceptor::MultiEndpointAcceptor(
    AbstractRequestHandler* requestHandler)
    :
    m_requestHandler(requestHandler)
{
}

network::server::Statistics MultiEndpointAcceptor::statistics() const
{
    return m_multiAddressHttpServer->statistics();
}

HttpStatistics MultiEndpointAcceptor::httpStatistics() const
{
    return m_httpStatsProvider
        ? m_httpStatsProvider->httpStatistics()
        : HttpStatistics();
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

    initializeHttpStatisticsProvider();

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

const std::vector<SocketAddress>& MultiEndpointAcceptor::endpoints() const
{
    return m_endpoints;
}

const std::vector<SocketAddress>& MultiEndpointAcceptor::sslEndpoints() const
{
    return m_sslEndpoints;
}

std::unique_ptr<MultiEndpointAcceptor::MultiHttpServer>
    MultiEndpointAcceptor::startHttpServer(
        const std::vector<network::SocketAddress>& endpoints)
{
    auto server = startServer(endpoints);
    if (server)
        m_endpoints = server->endpoints();
    return server;
}

std::unique_ptr<MultiEndpointAcceptor::MultiHttpServer>
    MultiEndpointAcceptor::startHttpsServer(
        const std::vector<network::SocketAddress>& endpoints)
{
    auto server = startServer(endpoints, ssl::Context::instance());
    if (server)
        m_sslEndpoints = server->endpoints();
    return server;
}

template<typename... Args>
std::unique_ptr<MultiEndpointAcceptor::MultiHttpServer>
    MultiEndpointAcceptor::startServer(
        const std::vector<network::SocketAddress>& endpoints,
        Args&&... args)
{
    auto multiAddressHttpServer = std::make_unique<MultiHttpServer>(
        m_requestHandler,
        std::forward<Args>(args)...);

    if (!multiAddressHttpServer->bind(endpoints))
        return nullptr;

    return multiAddressHttpServer;
}

void MultiEndpointAcceptor::initializeHttpStatisticsProvider()
{
    if (!m_multiAddressHttpServer)
        return;

    std::vector<const AbstractHttpStatisticsProvider*> providers;
    m_multiAddressHttpServer->forEachListener(
        [&providers](const auto& listener)
        {
            providers.emplace_back(listener);
        });

    m_httpStatsProvider = std::make_unique<SummingStatisticsProvider>(std::move(providers));
}

} // namespace nx::network::http::server
