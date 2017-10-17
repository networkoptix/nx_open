#pragma once

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/retry_timer.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/utils/std/future.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "mediator_client_connections.h"
#include "mediator_server_connections.h"

namespace nx {

namespace network { namespace cloud { class ConnectionMediatorUrlFetcher; } }

namespace hpm {
namespace api {

class NX_NETWORK_API AbstractMediatorConnector
{
public:
    virtual ~AbstractMediatorConnector() = default;

    /** Provides client related functionality. */
    virtual std::unique_ptr<MediatorClientTcpConnection> clientConnection() = 0;
    /** Provides server-related functionality. */
    virtual std::unique_ptr<MediatorServerTcpConnection> systemConnection() = 0;
    virtual boost::optional<SocketAddress> udpEndpoint() const = 0;
    virtual bool isConnected() const = 0;
};

class NX_NETWORK_API MediatorConnector:
    public AbstractMediatorConnector,
    public AbstractCloudSystemCredentialsProvider,
    public network::aio::BasicPollable
{
public:
    MediatorConnector();
    virtual ~MediatorConnector() override;

    MediatorConnector(MediatorConnector&&) = delete;
    MediatorConnector& operator=(MediatorConnector&&) = delete;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /**
     * Has to be called to enable cloud functionality for application.
     * E.g., initiates mediator address resolving.
     */
    void enable(bool waitComplete = false);

    /** Provides client related functionality */
    virtual std::unique_ptr<MediatorClientTcpConnection> clientConnection() override;

    /** Provides system related functionality */
    virtual std::unique_ptr<MediatorServerTcpConnection> systemConnection() override;

    /**
     * NOTE: Mediator url resolution will still happen by referring to specified address.
     */
    void mockupCloudModulesXmlUrl(const QUrl& cloudModulesXmlUrl);
    /**
     * Injects mediator url.
     * As a result, no mediator url resolution will happen.
     */
    void mockupMediatorUrl(const utils::Url &mediatorUrl);

    void setSystemCredentials(boost::optional<SystemCredentials> value);
    virtual boost::optional<SystemCredentials> getSystemCredentials() const override;

    virtual boost::optional<SocketAddress> udpEndpoint() const override;
    virtual bool isConnected() const override;

    static void setStunClientSettings(stun::AbstractAsyncClient::Settings stunClientSettings);

private:
    mutable QnMutex m_mutex;
    boost::optional<SystemCredentials> m_credentials;

    boost::optional<nx::utils::promise<bool>> m_promise;
    boost::optional<nx::utils::future<bool>> m_future;

    std::shared_ptr<stun::AsyncClientWithHttpTunneling> m_stunClient;
    std::unique_ptr<nx::network::cloud::ConnectionMediatorUrlFetcher> m_mediatorUrlFetcher;
    boost::optional<nx::utils::Url> m_mediatorUrl;
    boost::optional<SocketAddress> m_mediatorUdpEndpoint;
    std::unique_ptr<nx::network::RetryTimer> m_fetchEndpointRetryTimer;

    virtual void stopWhileInAioThread() override;

    void fetchEndpoint();
    void connectToMediatorAsync();
};

} // namespace api
} // namespace hpm
} // namespace nx
