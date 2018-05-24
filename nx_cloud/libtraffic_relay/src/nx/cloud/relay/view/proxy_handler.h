#pragma once

#include <nx/network/http/server/proxy/proxy_handler.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class ProxyHandler:
    public network::http::server::proxy::AbstractProxyHandler
{
    using base_type = network::http::server::proxy::AbstractProxyHandler;

public:
    ProxyHandler(relaying::AbstractListeningPeerPool* listeningPeerPool);

protected:
    virtual TargetHost cutTargetFromRequest(
        const nx::network::http::HttpServerConnection& connection,
        nx::network::http::Request* const request) override;

    virtual std::unique_ptr<network::aio::AbstractAsyncConnector>
        createTargetConnector() override;

private:
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;

    std::string extractOnlineHostName(const std::string& hostHeader);
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
