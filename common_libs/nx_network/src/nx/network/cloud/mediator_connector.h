#ifndef NX_CC_MEDIATOR_CONNECTOR_H
#define NX_CC_MEDIATOR_CONNECTOR_H

#include <future>

#include <boost/optional.hpp>

#include <nx/network/stun/async_client.h>
#include <nx/network/aio/timer.h>
#include <nx/network/retry_timer.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "cdb_endpoint_fetcher.h"
#include "mediator_connections.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API MediatorConnector
:
    public AbstractCloudSystemCredentialsProvider,
    public QnStoppableAsync
{
public:
    MediatorConnector();

    /** Shall be called to enable cloud functionality for application */
    void enable( bool waitComplete = false );

    /** Provides client related functionality */
    std::shared_ptr<MediatorClientTcpConnection> clientConnection();

    /** Provides system related functionality */
    std::shared_ptr<MediatorServerTcpConnection> systemConnection();

    /** Injects mediator address (tests only) */
    void mockupAddress( SocketAddress address );

    void setSystemCredentials( boost::optional<SystemCredentials> value );
    virtual boost::optional<SystemCredentials> getSystemCredentials() const;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    boost::optional<SocketAddress> mediatorAddress() const;

private:
    void fetchEndpoint();

    mutable QnMutex m_mutex;
    bool m_isTerminating;
    boost::optional< SystemCredentials > m_credentials;

    boost::optional< std::promise< bool > > m_promise;
    boost::optional< std::future< bool > > m_future;

    std::shared_ptr< stun::AbstractAsyncClient > m_stunClient;
    nx::network::cloud::CloudModuleEndPointFetcher m_endpointFetcher;
    nx::network::aio::Timer m_timer;
    boost::optional<SocketAddress> m_mediatorAddress;
    nx::network::RetryTimer m_fetchEndpointRetryTimer;
};

} // namespace api
} // namespace hpm
} // namespace nx

#endif // NX_CC_MEDIATOR_CONNECTOR_H
