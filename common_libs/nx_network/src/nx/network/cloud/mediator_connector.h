#pragma once

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/retry_timer.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/std/future.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "cloud_module_url_fetcher.h"
#include "mediator_client_connections.h"
#include "mediator_server_connections.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API MediatorConnector:
    public AbstractCloudSystemCredentialsProvider,
    public network::aio::BasicPollable
{
public:
    MediatorConnector();
    virtual ~MediatorConnector() override;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /** Shall be called to enable cloud functionality for application */
    void enable( bool waitComplete = false );

    /** Provides client related functionality */
    std::unique_ptr<MediatorClientTcpConnection> clientConnection();

    /** Provides system related functionality */
    std::unique_ptr<MediatorServerTcpConnection> systemConnection();

    /** Injects mediator address (tests only) */
    void mockupAddress( SocketAddress address, bool suppressWarning = false );
    void mockupAddress( const MediatorConnector& connector );

    void setSystemCredentials( boost::optional<SystemCredentials> value );
    virtual boost::optional<SystemCredentials> getSystemCredentials() const;

    boost::optional<SocketAddress> mediatorAddress() const;

    static void setStunClientSettings(stun::AbstractAsyncClient::Settings stunClientSettings);

private:
    void fetchEndpoint();

    mutable QnMutex m_mutex;
    boost::optional< SystemCredentials > m_credentials;

    boost::optional< nx::utils::promise< bool > > m_promise;
    boost::optional< nx::utils::future< bool > > m_future;

    std::shared_ptr< stun::AbstractAsyncClient > m_stunClient;
    std::unique_ptr<nx::network::cloud::ConnectionMediatorUrlFetcher> m_endpointFetcher;
    boost::optional<SocketAddress> m_mediatorAddress;
    std::unique_ptr<nx::network::RetryTimer> m_fetchEndpointRetryTimer;

    virtual void stopWhileInAioThread() override;
};

} // namespace api
} // namespace hpm
} // namespace nx
