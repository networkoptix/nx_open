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
    public AbstractHttpRequestHandler
{
public:
    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

    void setTargetHostConnectionInactivityTimeout(
        std::optional<std::chrono::milliseconds> timeout);

    void setSslHandshakeTimeout(
        std::optional<std::chrono::milliseconds> timeout);

protected:
    struct NX_NETWORK_API TargetHost
    {
        network::SocketAddress target;
        SslMode sslMode = SslMode::followIncomingConnection;

        TargetHost() = default;
        TargetHost(network::SocketAddress target);
    };

    using ProxyTargetDetectedHandler = nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value resultCode,
        TargetHost proxyTarget)>;

    /**
     * NOTE: It is allowed to invoke handler right within this function.
     * There must not be any activity after invoking handler since
     * AbstractProxyHandler can be freed.
     */
    virtual void detectProxyTarget(
        const nx::network::http::HttpServerConnection& connection,
        nx::network::http::Request* const request,
        ProxyTargetDetectedHandler handler) = 0;

    virtual std::unique_ptr<aio::AbstractAsyncConnector>
        createTargetConnector() = 0;

private:
    nx::network::http::Request m_request;
    nx::network::http::RequestProcessedHandler m_requestCompletionHandler;
    SocketAddress m_requestSourceEndpoint;
    aio::AbstractAioThread* m_httpConnectionAioThread = nullptr;
    std::unique_ptr<nx::network::http::server::proxy::ProxyWorker> m_requestProxyWorker;
    TargetHost m_targetHost;
    bool m_isIncomingConnectionEncrypted = false;
    bool m_sslConnectionRequired = false;
    std::unique_ptr<aio::AbstractAsyncConnector> m_targetPeerConnector;
    std::optional<std::chrono::milliseconds> m_targetConnectionInactivityTimeout;
    std::optional<std::chrono::milliseconds> m_sslHandshakeTimeout;
    std::chrono::milliseconds m_connectionSendTimeout = network::kNoTimeout;
    std::unique_ptr<AbstractEncryptedStreamSocket> m_encryptedConnection;

    void startProxying(
        nx::network::http::StatusCode::Value resultCode,
        TargetHost proxyTarget);

    void onConnected(
        const network::SocketAddress& targetAddress,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<network::AbstractStreamSocket> connection);

    void proxyRequestToTarget(std::unique_ptr<AbstractStreamSocket> connection);

    void sendTargetServerResponse(
        nx::network::http::RequestResult requestResult,
        boost::optional<nx::network::http::Response> response);

    void establishSecureConnectionToTheTarget(
        std::unique_ptr<AbstractStreamSocket> connection);

    void processSslHandshakeResult(SystemError::ErrorCode handshakeResult);
};

} // namespace proxy
} // namespace server
} // namespace http
} // namespace network
} // namespace nx
