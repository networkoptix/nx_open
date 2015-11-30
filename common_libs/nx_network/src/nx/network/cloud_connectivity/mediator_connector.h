#ifndef NX_CC_MEDIATOR_CONNECTOR_H
#define NX_CC_MEDIATOR_CONNECTOR_H

#include <future>

#include <nx/network/stun/async_client.h>

#include "cdb_endpoint_fetcher.h"

namespace nx {
    class SocketGlobals;
}   //nx

namespace nx {
namespace cc {

class NX_NETWORK_API MediatorConnector
{
    MediatorConnector();
    ~MediatorConnector();

    friend class ::nx::SocketGlobals;

public:
    /** Shell be called to enable cloud functionality for application */
    void enable( bool waitComplete = false );

    /** Returns client if endpoint is resolved, nullptr otherwise */
    stun::AsyncClient* client();

    /** Injects mediator address (tests only) */
    void mockupAddress( SocketAddress address );

private:
    mutable QnMutex m_mutex;
    bool m_isTerminating;

    boost::optional< std::promise< bool > > m_promise;
    boost::optional< SocketAddress > m_endpoint;
    std::unique_ptr< stun::AsyncClient > m_client;
    CloudModuleEndPointFetcher m_endpointFetcher;
    std::unique_ptr< AbstractStreamSocket > m_timerSocket;
};

}   //cc
}   //nx

#endif // NX_CC_MEDIATOR_CONNECTOR_H
