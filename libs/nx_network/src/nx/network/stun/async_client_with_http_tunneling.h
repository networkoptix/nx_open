// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/thread/mutex.h>

#include "async_client.h"

namespace nx {
namespace network {
namespace stun {

/**
 * Establishes STUN over HTTP tunnel if http url is supplied.
 */
class NX_NETWORK_API AsyncClientWithHttpTunneling:
    public AbstractAsyncClient
{
    using base_type = AbstractAsyncClient;

public:
    AsyncClientWithHttpTunneling(Settings settings = Settings());
    virtual ~AsyncClientWithHttpTunneling();

    AsyncClientWithHttpTunneling(const AsyncClientWithHttpTunneling&) = delete;
    AsyncClientWithHttpTunneling(AsyncClientWithHttpTunneling&&) = delete;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    /**
     * @param url http and stun scheme is supported.
     */
    virtual void connect(const nx::utils::Url& url, ConnectHandler handler) override;

    virtual void setIndicationHandler(
        int method,
        IndicationHandler handler,
        void* client = nullptr) override;

    virtual void addOnReconnectedHandler(
        ReconnectHandler handler,
        void* client = nullptr) override;

    virtual void setOnConnectionClosedHandler(
        OnConnectionClosedHandler onConnectionClosedHandler) override;

    virtual void sendRequest(
        Message request,
        RequestHandler handler,
        void* client = nullptr) override;

    virtual bool addConnectionTimer(
        std::chrono::milliseconds period,
        TimerHandler handler,
        void* client) override;

    virtual SocketAddress localAddress() const override;
    virtual SocketAddress remoteAddress() const override;

    virtual void closeConnection(SystemError::ErrorCode errorCode) override;

    virtual void cancelHandlers(
        void* client,
        nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual void cancelHandlersSync(void* client) override;

    virtual void setKeepAliveOptions(KeepAliveOptions options) override;

    void setTunnelValidatorFactory(http::tunneling::TunnelValidatorFactoryFunc func);

    void setHttpTunnelingClientHeaders(http::HttpHeaders headers);

private:
    struct HandlerContext
    {
        IndicationHandler handler;
        void* client = nullptr;
    };

    struct RequestContext
    {
        Message request;
        RequestHandler handler;
        void* client = nullptr;
    };

    Settings m_settings;
    std::unique_ptr<AsyncClient> m_stunClient;
    std::unique_ptr<nx::network::http::tunneling::Client> m_httpTunnelingClient;
    ConnectHandler m_httpTunnelEstablishedHandler;
    /** map<stun method, handler> */
    std::map<int, HandlerContext> m_indicationHandlers;
    mutable nx::Mutex m_mutex;
    nx::utils::Url m_url;
    std::map<void*, ReconnectHandler> m_reconnectHandlers;
    nx::network::RetryTimer m_reconnectTimer;
    nx::utils::InterruptionFlag m_destructionFlag;
    std::optional<KeepAliveOptions> m_keepAliveOptions;
    int m_requestIdSequence = 0;
    /** map<request id, request context>. */
    std::map<int, RequestContext> m_activeRequests;
    OnConnectionClosedHandler m_connectionClosedHandler;
    ConnectHandler m_userConnectHandler;
    http::tunneling::TunnelValidatorFactoryFunc m_tunnelValidatorFactory;
    http::HttpHeaders m_customHeaders;

    virtual void stopWhileInAioThread() override;

    void connectInternal(
        const nx::Locker<nx::Mutex>& /*lock*/,
        ConnectHandler handler);

    void applyConnectionSettings();

    void createStunClient(
        const nx::Locker<nx::Mutex>& /*lock*/,
        std::unique_ptr<AbstractStreamSocket> connection);

    void dispatchIndication(nx::network::stun::Message indication);
    void sendPendingRequests();

    void openHttpTunnel(
        const nx::Locker<nx::Mutex>&,
        const nx::utils::Url& url,
        ConnectHandler handler);

    void onOpenHttpTunnelCompletion(
        nx::network::http::tunneling::OpenTunnelResult tunnelResult);

    void onRequestCompleted(
        SystemError::ErrorCode sysErrorCode,
        nx::network::stun::Message response,
        int requestId);

    template<typename Dictionary> void cancelUserHandlers(
        Dictionary& dictionary, void* client);

    void onStunConnectionClosed(SystemError::ErrorCode closeReason);
    void scheduleReconnect();
    void reconnect();
    void onReconnectDone(SystemError::ErrorCode sysErrorCode);
    void reportReconnect();
};

} // namespace stun
} // namespace network
} // namespace nx
