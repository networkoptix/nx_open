#pragma once

#include <nx/network/aio/abstract_async_connector.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/std/optional.h>

#include "proxy_worker.h"
#include "../abstract_http_request_handler.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

enum class SslMode
{
    followIncomingConnection,
    enabled,
    disabled,
};

class NX_NETWORK_API AbstractProxyHandler:
    public AbstractHttpRequestHandler,
    public AbstractResponseSender
{
public:
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

    virtual void sendResponse(
        nx::network::http::RequestResult requestResult,
        boost::optional<nx::network::http::Response> response) override;

    void setTargetHostConnectionInactivityTimeout(
        std::optional<std::chrono::milliseconds> timeout);

protected:
    struct NX_NETWORK_API TargetHost
    {
        nx::network::http::StatusCode::Value status =
            nx::network::http::StatusCode::notImplemented;
        network::SocketAddress target;
        SslMode sslMode = SslMode::followIncomingConnection;

        TargetHost() = default;
        TargetHost(
            nx::network::http::StatusCode::Value status,
            network::SocketAddress target = {});
    };

    virtual TargetHost cutTargetFromRequest(
        const nx::network::http::HttpServerConnection& connection,
        nx::network::http::Request* const request) = 0;

    virtual std::unique_ptr<aio::AbstractAsyncConnector>
        createTargetConnector() = 0;

private:
    nx::network::http::Request m_request;
    nx::network::http::RequestProcessedHandler m_requestCompletionHandler;
    std::unique_ptr<nx::network::http::server::proxy::ProxyWorker> m_requestProxyWorker;
    TargetHost m_targetHost;
    bool m_sslConnectionRequired = false;
    std::unique_ptr<aio::AbstractAsyncConnector> m_targetPeerConnector;
    std::optional<std::chrono::milliseconds> m_targetConnectionInactivityTimeout;

    void onConnected(
        const network::SocketAddress& targetAddress,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<network::AbstractStreamSocket> connection);
};

} // namespace proxy
} // namespace server
} // namespace http
} // namespace network
} // namespace nx
