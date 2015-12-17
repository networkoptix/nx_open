#ifndef NX_CC_MEDIATOR_CONNECTOR_H
#define NX_CC_MEDIATOR_CONNECTOR_H

#include <future>

#include <nx/network/stun/async_client.h>

#include "cdb_endpoint_fetcher.h"
#include "mediator_connections.h"

namespace nx {
    class SocketGlobals;
}   //nx

namespace nx {
namespace cc {

class NX_NETWORK_API MediatorConnector
    : public QnStoppableAsync
{
    MediatorConnector();
    friend class ::nx::SocketGlobals;

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
    struct SystemCredentials
    {
        String systemId, serverId, key;
        bool operator ==( const SystemCredentials& rhs ) const;
    };

    void setSystemCredentials( boost::optional<SystemCredentials> value );
    boost::optional<SystemCredentials> getSystemCredentials();

    void pleaseStop( std::function<void()> handler ) override;

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

}   //cc
}   //nx

#endif // NX_CC_MEDIATOR_CONNECTOR_H
