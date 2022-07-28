// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/network/aio/abstract_async_connector.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/ssl/helpers.h>

#include "proxy_worker.h"
#include "../abstract_http_request_handler.h"

namespace nx::network::http::server::proxy {

enum class SslMode
{
    followIncomingConnection,
    enabled,
    disabled,
};

/**
 * Implements HTTP proxy. Every request that is received by this handler is treated as a request
 * to some other server.
 * The endpoint of the target server is detected by a pure virtual call
 * AbstractProxyHandler::detectProxyTarget.
 *
 * Proxying of both finite and infinite request/response bodies is supported. Message bodies can be
 * represented with both identity and chunked encodings.
 *
 * This class detects proxy target, establishes connection to the target through the virtual call
 * AbstractProxyHandler::createTargetConnector and delegates the actual data proxying to the
 * ProxyWorker class.
 *
 * Note: the class instance lives until response from the target server has been received.
 *
 * Note: as of now, connections to the target server are not reused. So, every request uses its own
 * connection. This is subject to change.
 */
class NX_NETWORK_API AbstractProxyHandler:
    public RequestHandlerWithContext
{
public:
    struct NX_NETWORK_API TargetHost
    {
        network::SocketAddress target;
        SslMode sslMode = SslMode::followIncomingConnection;

        /**
         * Filled in case a redirect is needed to reach the requested target.
         * If filled then proxy request will complete with a redirect to the specified URL.
         */
        std::optional<nx::utils::Url> redirectLocation;

        TargetHost() = default;
        TargetHost(network::SocketAddress target);
    };

public:
    AbstractProxyHandler();

    virtual void processRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) override;

    void setTargetHostConnectionInactivityTimeout(
        std::optional<std::chrono::milliseconds> timeout);

    void setSslHandshakeTimeout(
        std::optional<std::chrono::milliseconds> timeout);

    void setCertificateChainVerificationCallback(ssl::VerifyCertificateAsyncFunc cb);

protected:
    using ProxyTargetDetectedHandler = nx::utils::MoveOnlyFunc<void(
        StatusCode::Value resultCode,
        TargetHost proxyTarget)>;

    /**
     * @param request The implementation is allowed to modify this request.
     * NOTE: It is allowed to invoke handler right within this function.
     * There must not be any activity after invoking handler since
     * AbstractProxyHandler can be freed.
     */
    virtual void detectProxyTarget(
        const nx::network::http::ConnectionAttrs& connAttrs,
        const nx::network::SocketAddress& requestSource,
        nx::network::http::Request* const request,
        ProxyTargetDetectedHandler handler) = 0;

    /**
     * By default, a regular socket is created.
     * Override this method to introduce a custom connection logic.
     * E.g., reverse proxy server has to lookup connection in some connection pool.
     */
    virtual std::unique_ptr<aio::AbstractAsyncConnector> createTargetConnector();

private:
    Request m_request;
    std::unique_ptr<AbstractMsgBodySourceWithCache> m_requestBody;
    RequestProcessedHandler m_requestCompletionHandler;
    SocketAddress m_requestSourceEndpoint;
    aio::AbstractAioThread* m_httpConnectionAioThread = nullptr;
    std::unique_ptr<server::proxy::ProxyWorker> m_requestProxyWorker;
    TargetHost m_targetHost;
    bool m_isIncomingConnectionEncrypted = false;
    bool m_sslConnectionRequired = false;
    std::optional<std::chrono::milliseconds> m_targetConnectionInactivityTimeout;
    std::optional<std::chrono::milliseconds> m_sslHandshakeTimeout;
    std::chrono::milliseconds m_connectionSendTimeout = network::kNoTimeout;
    std::unique_ptr<AbstractEncryptedStreamSocket> m_encryptedConnection;
    ssl::VerifyCertificateAsyncFunc m_certificateChainVerificationCallback = acceptAllCertificates;

    void fixRequestHeaders();

    void startProxying(
        StatusCode::Value resultCode,
        TargetHost proxyTarget);

    void onCacheLookupFinished(
        std::unique_ptr<AbstractStreamSocket> socket);

    void onConnected(
        const network::SocketAddress& targetAddress,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<network::AbstractStreamSocket> connection,
        std::unique_ptr<aio::AbstractAsyncConnector> connector);

    void proxyRequestToTarget(std::unique_ptr<AbstractStreamSocket> connection);

    void sendTargetServerResponse(RequestResult requestResult);

    void removeKeepAliveConnectionHeaders(HttpHeaders* responseHeaders);

    void establishSecureConnectionToTheTarget(
        std::unique_ptr<AbstractStreamSocket> connection);

    void processSslHandshakeResult(SystemError::ErrorCode handshakeResult);

    static void acceptAllCertificates(
        const ssl::CertificateChainView&,
        nx::utils::MoveOnlyFunc<void(bool)>);
};

//-------------------------------------------------------------------------------------------------

/**
 * Implements regular HTTP proxy.
 * Host to proxy to is taken from the request URL.
 */
class NX_NETWORK_API ProxyHandler:
    public AbstractProxyHandler
{
protected:
    virtual void detectProxyTarget(
        const ConnectionAttrs& connAttrs,
        const SocketAddress& requestSource,
        Request* const request,
        ProxyTargetDetectedHandler handler) override;
};

} // namespace nx::network::http::server::proxy
