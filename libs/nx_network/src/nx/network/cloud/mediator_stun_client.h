#pragma once

#include <optional>

#include <nx/network/http/http_types.h>
#include <nx/network/stun/async_client_delegate.h>
#include <nx/network/stun/server_aliveness_tester.h>

namespace nx::hpm::api {

class MediatorEndpointProvider;

/**
 * Adds support for calling AbstractAsyncClient::sendRequest before AbstractAsyncClient::connect.
 * It implicitely fetches endpoint from MediatorEndpointProvider and
 * calls AbstractAsyncClient::connect.
 * Also, it implements custom keep alive probes.
 */
class NX_NETWORK_API MediatorStunClient:
    public nx::network::stun::AsyncClientDelegate
{
    using base_type = nx::network::stun::AsyncClientDelegate;

public:
    MediatorStunClient(
        AbstractAsyncClient::Settings settings,
        MediatorEndpointProvider* endpointProvider);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(
        const nx::utils::Url& url,
        ConnectHandler handler) override;

    virtual void sendRequest(
        nx::network::stun::Message request,
        RequestHandler handler,
        void* client = nullptr) override;

    virtual void setOnConnectionClosedHandler(
        OnConnectionClosedHandler onConnectionClosedHandler) override;

    virtual void setKeepAliveOptions(
        nx::network::KeepAliveOptions options) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct RequestContext
    {
        nx::network::stun::Message request;
        RequestHandler handler;
        void* client = nullptr;
    };

    MediatorEndpointProvider* m_endpointProvider = nullptr;
    bool m_urlKnown = false;
    std::vector<RequestContext> m_postponedRequests;
    std::unique_ptr<nx::network::stun::ServerAlivenessTester> m_alivenessTester;
    std::optional<nx::network::KeepAliveOptions> m_keepAliveOptions;
    OnConnectionClosedHandler m_onConnectionClosedHandler;
    bool m_connected = false;

    void handleConnectionClosure(SystemError::ErrorCode reason);

    void onFetchEndpointCompletion(
        nx::network::http::StatusCode::Value resultCode);

    void failPendingRequests(SystemError::ErrorCode resultCode);

    void sendPendingRequests();

    void handleConnectCompletion(
        ConnectHandler handler,
        SystemError::ErrorCode resultCode);

    void startKeepAliveProbing();
    void handleAlivenessTestFailure();

    void stopKeepAliveProbing();
};

} // namespace nx::hpm::api
