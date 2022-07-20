// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/network/connection_server/multi_address_server.h>

#include "http_stream_socket_server.h"

namespace nx::network::http::server {

class NX_NETWORK_API MultiEndpointAcceptor:
    public network::server::AbstractStatisticsProvider,
    public network::http::server::AbstractHttpStatisticsProvider
{
public:
    MultiEndpointAcceptor(AbstractRequestHandler* requestHandler);

    virtual network::server::Statistics statistics() const override;
    virtual HttpStatistics httpStatistics() const override;

    void pleaseStopSync();

    bool bind(
        const std::vector<SocketAddress>& endpoints,
        const std::vector<SocketAddress>& sslEndpoints);

    bool listen(int backlogSize = AbstractStreamServerSocket::kDefaultBacklogSize);

    const std::vector<SocketAddress>& endpoints() const;
    const std::vector<SocketAddress>& sslEndpoints() const;

    template<typename... Args>
    void forEachListener(Args&&... args)
    {
        m_multiAddressHttpServer->forEachListener(std::forward<decltype(args)>(args)...);
    }

private:
    using MultiHttpServer =
        network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

    AbstractRequestHandler* m_requestHandler = nullptr;
    std::vector<SocketAddress> m_endpoints;
    std::vector<SocketAddress> m_sslEndpoints;
    std::unique_ptr<MultiHttpServer> m_multiAddressHttpServer;
    std::unique_ptr<AggregateHttpStatisticsProvider> m_httpStatsProvider;

    std::unique_ptr<MultiHttpServer> startHttpServer(
        const std::vector<network::SocketAddress>& endpoints);

    std::unique_ptr<MultiHttpServer> startHttpsServer(
        const std::vector<network::SocketAddress>& endpoints);

    template<typename... Args>
    std::unique_ptr<MultiHttpServer> startServer(
        const std::vector<network::SocketAddress>& endpoints,
        Args&&... args);

    void initializeHttpStatisticsProvider();
};

} // namespace nx::network::http::server
