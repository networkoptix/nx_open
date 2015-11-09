#ifndef NX_CC_MEDIATOR_CONNECTOR_H
#define NX_CC_MEDIATOR_CONNECTOR_H

#include <future>

#include "utils/network/stun/async_client.h"

#include "cdb_endpoint_fetcher.h"

namespace nx {
    class SocketGlobals;
}   //nx

namespace nx {
namespace cc {

class MediatorConnector
{
    MediatorConnector();
    ~MediatorConnector();

    friend class ::nx::SocketGlobals;

public:
    /** Shell be called to enable cloud functionality for application */
    void enable( bool waitComplete = false );

    /** Constructs client if endpoint is resolved, nullptr otherwise */
    std::unique_ptr< stun::AsyncClient > client();

    /** Injects mediator address (tests only) */
    void mockupAddress( SocketAddress address );

private:
    mutable QnMutex m_mutex;
    bool m_isTerminating;

    boost::optional< std::promise< bool > > m_promise;
    boost::optional< SocketAddress > m_endpoint;
    CloudModuleEndPointFetcher m_endpointFetcher;
    std::unique_ptr< AbstractStreamSocket > m_timerSocket;
};

}   //cc
}   //nx

#endif // NX_CC_MEDIATOR_CONNECTOR_H
