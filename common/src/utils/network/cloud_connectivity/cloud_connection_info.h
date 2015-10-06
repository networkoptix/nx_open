#ifndef NX_CC_CLOUD_CONNECTION_INFO_H
#define NX_CC_CLOUD_CONNECTION_INFO_H

#include "cdb_endpoint_fetcher.h"

namespace nx {
    class SocketGlobals;
}   //nx

namespace nx {
namespace cc {

// TODO: generalize to keep all cloud related information
class CloudConnectionInfo
{
    CloudConnectionInfo();
    friend class ::nx::SocketGlobals;

public:
    /** Shell be called to enable cloud functionality for application */
    void enableMediator();

    /** Returns mediator address if fetched, boost::none otherwise */
    boost::optional< SocketAddress > mediatorAddress() const;

private:
    mutable QnMutex m_mutex;
    boost::optional< SocketAddress > m_mediatorAddress;
    CloudModuleEndPointFetcher m_mediatorEndpointFetcher;
};

}   //cc
}   //nx

#endif // NX_CC_CLOUD_CONNECTION_INFO_H
