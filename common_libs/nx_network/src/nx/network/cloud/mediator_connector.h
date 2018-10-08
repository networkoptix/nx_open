#pragma once

#include <optional>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/retry_timer.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_delegate.h>
#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/utils/std/future.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "mediator_client_connections.h"
#include "mediator_server_connections.h"

namespace nx {

namespace network::cloud { class ConnectionMediatorUrlFetcher; }

namespace hpm {
namespace api {

class DelayedConnectStunClient;
class MediatorEndpointProvider;

using MediatorAvailabilityChangedHandler =
    nx::utils::MoveOnlyFunc<void(bool /*isMediatorAvailable*/)>;

class NX_NETWORK_API AbstractMediatorConnector:
    public network::aio::BasicPollable
{
public:
    virtual ~AbstractMediatorConnector() = default;

    /** Provides client related functionality. */
    virtual std::unique_ptr<MediatorClientTcpConnection> clientConnection() = 0;

    /** Provides server-related functionality. */
    virtual std::unique_ptr<MediatorServerTcpConnection> systemConnection() = 0;

    /**
     * @param endpoint Defined only if resultCode is SystemError::noError.
     */
    virtual void fetchUdpEndpoint(nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        nx::network::SocketAddress /*endpoint*/)> handler) = 0;

    /**
     * Provides endpoint without blocking.
     */
    virtual std::optional<nx::network::SocketAddress> udpEndpoint() const = 0;

    virtual void setOnMediatorAvailabilityChanged(
        MediatorAvailabilityChangedHandler handler) = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API MediatorConnector:
    public AbstractMediatorConnector,
    public AbstractCloudSystemCredentialsProvider
{
public:
    MediatorConnector(const std::string& cloudHost);
    virtual ~MediatorConnector() override;

    MediatorConnector(MediatorConnector&&) = delete;
    MediatorConnector& operator=(MediatorConnector&&) = delete;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /** Provides client related functionality */
    virtual std::unique_ptr<MediatorClientTcpConnection> clientConnection() override;

    /** Provides system related functionality */
    virtual std::unique_ptr<MediatorServerTcpConnection> systemConnection() override;

    /**
     * NOTE: Mediator url resolution will still happen by referring to specified address.
     */
    void mockupCloudModulesXmlUrl(const utils::Url &cloudModulesXmlUrl);
    
    /**
     * Injects mediator url.
     * As a result, no mediator url resolution will happen.
     */
    void mockupMediatorUrl(
        const utils::Url& mediatorUrl,
        const network::SocketAddress stunUdpEndpoint);

    void setSystemCredentials(std::optional<SystemCredentials> value);

    virtual std::optional<SystemCredentials> getSystemCredentials() const override;

    virtual void fetchUdpEndpoint(nx::utils::MoveOnlyFunc<void(
        nx::network::http::StatusCode::Value /*resultCode*/,
        nx::network::SocketAddress /*endpoint*/)> handler) override;

    virtual std::optional<nx::network::SocketAddress> udpEndpoint() const override;

    virtual void setOnMediatorAvailabilityChanged(
        MediatorAvailabilityChangedHandler handler) override;

    static void setStunClientSettings(network::stun::AbstractAsyncClient::Settings stunClientSettings);

private:
    mutable QnMutex m_mutex;
    std::optional<SystemCredentials> m_credentials;

    std::unique_ptr<MediatorEndpointProvider> m_mediatorEndpointProvider;
    std::shared_ptr<DelayedConnectStunClient> m_stunClient;
    std::optional<nx::utils::Url> m_mockedUpMediatorUrl;
    std::unique_ptr<nx::network::RetryTimer> m_fetchEndpointRetryTimer;
    MediatorAvailabilityChangedHandler m_mediatorAvailabilityChangedHandler;

    virtual void stopWhileInAioThread() override;

    void connectToMediatorAsync();

    void establishTcpConnectionToMediatorAsync();

    void reconnectToMediator();
};

//-------------------------------------------------------------------------------------------------

/**
 * Adds support for calling AbstractAsyncClient::sendRequest before AbstractAsyncClient::connect.
 * It implicitely fetches endpoint from MediatorEndpointProvider and 
 * calls AbstractAsyncClient::connect.
 */
class DelayedConnectStunClient:
    public nx::network::stun::AsyncClientDelegate
{
    using base_type = nx::network::stun::AsyncClientDelegate;

public:
    DelayedConnectStunClient(
        MediatorEndpointProvider* endpointProvider,
        std::unique_ptr<nx::network::stun::AbstractAsyncClient> delegate);

    virtual void connect(
        const nx::utils::Url& url,
        ConnectHandler handler) override;

    virtual void sendRequest(
        nx::network::stun::Message request,
        RequestHandler handler,
        void* client) override;

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

    void onFetchEndpointCompletion(
        nx::network::http::StatusCode::Value resultCode);

    void failPendingRequests(SystemError::ErrorCode resultCode);

    void sendPendingRequests();
};

//-------------------------------------------------------------------------------------------------

// TODO: #ak Merge this class with ConnectionMediatorUrlFetcher.
class MediatorEndpointProvider:
    public nx::network::aio::BasicPollable
{
public:
    using base_type = nx::network::aio::BasicPollable;

    using FetchMediatorEndpointsCompletionHandler =
        nx::utils::MoveOnlyFunc<void(
            nx::network::http::StatusCode::Value /*resultCode*/)>;

    MediatorEndpointProvider(const std::string& cloudHost);

    virtual void bindToAioThread(
        network::aio::AbstractAioThread* aioThread) override;

    void mockupCloudModulesXmlUrl(const nx::utils::Url& cloudModulesXmlUrl);

    void mockupMediatorUrl(
        const nx::utils::Url& mediatorUrl,
        const network::SocketAddress stunUdpEndpoint);

    void fetchMediatorEndpoints(
        FetchMediatorEndpointsCompletionHandler handler);

    std::optional<nx::utils::Url> mediatorUrl() const;
    
    std::optional<network::SocketAddress> udpEndpoint() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const std::string m_cloudHost;
    mutable QnMutex m_mutex;
    std::vector<FetchMediatorEndpointsCompletionHandler> m_fetchMediatorEndpointsHandlers;
    std::unique_ptr<nx::network::cloud::ConnectionMediatorUrlFetcher> m_mediatorUrlFetcher;
    std::optional<nx::utils::Url> m_cloudModulesXmlUrl;
    std::optional<nx::utils::Url> m_mediatorUrl;
    std::optional<network::SocketAddress> m_mediatorUdpEndpoint;

    void initializeUrlFetcher();
};

} // namespace api
} // namespace hpm
} // namespace nx
