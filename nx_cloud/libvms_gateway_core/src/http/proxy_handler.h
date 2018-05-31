#pragma once

#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/network/http/server/proxy/proxy_worker.h>

#include "settings.h"
#include "target_peer_connector.h"

namespace nx {
namespace cloud {
namespace gateway {

namespace conf {

class RunTimeOptions;

} // namespace conf

class ProxyHandler:
    public nx::network::http::server::proxy::AbstractProxyHandler
{
    using base_type = nx::network::http::server::proxy::AbstractProxyHandler;

public:
    ProxyHandler(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions,
        relaying::AbstractListeningPeerPool* listeningPeerPool);

protected:
    virtual TargetHost cutTargetFromRequest(
        const nx::network::http::HttpServerConnection& connection,
        nx::network::http::Request* const request) override;

    virtual std::unique_ptr<nx::network::aio::AbstractAsyncConnector>
        createTargetConnector() override;

private:
    const conf::Settings& m_settings;
    const conf::RunTimeOptions& m_runTimeOptions;
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;

    TargetHost cutTargetFromUrl(nx::network::http::Request* const request);
    TargetHost cutTargetFromPath(nx::network::http::Request* const request);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
