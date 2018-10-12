#pragma once

#include <vector>

#include <nx/network/connection_server/multi_address_server.h>

#include "http_stream_socket_server.h"

namespace nx::network::http::server {

class NX_NETWORK_API MultiEndpointAcceptor:
    public network::server::AbstractStatisticsProvider
{
public:
    MultiEndpointAcceptor(
        server::AbstractAuthenticationManager* authenticationManager,
        AbstractMessageDispatcher* httpMessageDispatcher);

    virtual network::server::Statistics statistics() const override;

    void pleaseStopSync();

    bool bind(
        const std::vector<SocketAddress>& endpoints,
        const std::vector<SocketAddress>& sslEndpoints);

    bool listen(int backlogSize = AbstractStreamServerSocket::kDefaultBacklogSize);

    std::vector<SocketAddress> endpoints() const;
    std::vector<SocketAddress> sslEndpoints() const;

    template<typename... Args>
    void forEachListener(const Args&... args)
    {
        m_multiAddressHttpServer->forEachListener(args...);
    }

private:
    using MultiHttpServer =
        network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

    server::AbstractAuthenticationManager* m_authenticationManager = nullptr;
    AbstractMessageDispatcher* m_httpMessageDispatcher = nullptr;
    std::vector<SocketAddress> m_endpoints;
    std::vector<SocketAddress> m_sslEndpoints;
    std::unique_ptr<MultiHttpServer> m_multiAddressHttpServer;

    std::unique_ptr<MultiHttpServer> startHttpServer(
        const std::vector<network::SocketAddress>& endpoints);

    std::unique_ptr<MultiHttpServer> startHttpsServer(
        const std::vector<network::SocketAddress>& endpoints);

    std::unique_ptr<MultiHttpServer> startServer(
        const std::vector<network::SocketAddress>& endpoints,
        bool sslMode);
};

} // namespace nx::network::http::server
