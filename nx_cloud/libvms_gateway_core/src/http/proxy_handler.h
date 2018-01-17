#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
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
    public nx::network::http::AbstractHttpRequestHandler,
    public nx::network::http::server::proxy::AbstractResponseSender
{
public:
    ProxyHandler(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions,
        relaying::AbstractListeningPeerPool* listeningPeerPool);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

    virtual void sendResponse(
        nx::network::http::RequestResult requestResult,
        boost::optional<nx::network::http::Response> response) override;

private:
    struct TargetHost
    {
        nx::network::http::StatusCode::Value status = nx::network::http::StatusCode::notImplemented;
        network::SocketAddress target;
        conf::SslMode sslMode = conf::SslMode::followIncomingConnection;

        TargetHost() = default;
        TargetHost(nx::network::http::StatusCode::Value status, network::SocketAddress target = {});
    };

    const conf::Settings& m_settings;
    const conf::RunTimeOptions& m_runTimeOptions;
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;

    nx::network::http::Request m_request;
    nx::network::http::RequestProcessedHandler m_requestCompletionHandler;
    std::unique_ptr<nx::network::http::server::proxy::ProxyWorker> m_requestProxyWorker;
    TargetHost m_targetHost;
    bool m_sslConnectionRequired = false;
    std::unique_ptr<TargetPeerConnector> m_targetPeerConnector;

    TargetHost cutTargetFromRequest(
        const nx::network::http::HttpServerConnection& connection,
        nx::network::http::Request* const request);

    TargetHost cutTargetFromUrl(nx::network::http::Request* const request);
    TargetHost cutTargetFromPath(nx::network::http::Request* const request);

    void onConnected(
        const network::SocketAddress& targetAddress,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<network::AbstractStreamSocket> connection);
};

} // namespace gateway
} // namespace cloud
} // namespace nx
