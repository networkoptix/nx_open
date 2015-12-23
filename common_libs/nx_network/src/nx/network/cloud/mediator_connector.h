#ifndef NX_CC_MEDIATOR_CONNECTOR_H
#define NX_CC_MEDIATOR_CONNECTOR_H

#include <future>

#include <nx/network/stun/async_client.h>

#include "cdb_endpoint_fetcher.h"
#include "mediator_connections.h"

namespace nx {
namespace network {

class SocketGlobals;

} // namespace network
} // namespace nx

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API MediatorConnector
    : public QnStoppableAsync
{
    friend class ::nx::network::SocketGlobals;

public:
    /** Shell be called to enable cloud functionality for application */
    void enable( bool waitComplete = false );

    /** Provides client related functionality */
    std::shared_ptr<MediatorClientConnection> clientConnection();

    /** Provides system related functionality */
    std::shared_ptr<MediatorSystemConnection> systemConnection();

    /** Injects mediator address (tests only) */
    void mockupAddress( SocketAddress address );

    /** Authorization credentials for \class MediatorSystemConnection */
    struct NX_NETWORK_API SystemCredentials
    {
        String systemId, serverId, key;
        
        SystemCredentials();
        SystemCredentials(
            nx::String _systemId,
            nx::String _serverId,
            nx::String _key);
        bool operator ==( const SystemCredentials& rhs ) const;
    };

    void setSystemCredentials( boost::optional<SystemCredentials> value );
    boost::optional<SystemCredentials> getSystemCredentials();

    void pleaseStop( std::function<void()> handler ) override;

protected:
    MediatorConnector();

private:
    void fetchEndpoint();

    mutable QnMutex m_mutex;
    bool m_isTerminating;
    boost::optional< SystemCredentials > m_credentials;

    boost::optional< std::promise< bool > > m_promise;
    std::shared_ptr< stun::AsyncClient > m_stunClient;
    CloudModuleEndPointFetcher m_endpointFetcher;
    std::unique_ptr< AbstractStreamSocket > m_timerSocket;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_MEDIATOR_CONNECTOR_H
