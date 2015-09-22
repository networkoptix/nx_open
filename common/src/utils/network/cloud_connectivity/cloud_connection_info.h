#ifndef NX_CC_CLOUD_CONNECTION_INFO_H
#define NX_CC_CLOUD_CONNECTION_INFO_H

#include "cdb_endpoint_fetcher.h"

namespace nx {
namespace cc {

// TODO: generalize to keep all cloud related information
class CloudConnectionInfo
{
public:
    CloudConnectionInfo();

    /** Shell be called to enable cloud functionality for application */
    void enableMediator();

    /** Returns mediator address if fetched, boost::none otherwise */
    boost::optional< SocketAddress > mediatorAddress();

private:
    QnMutex m_mutex;
    CloudModuleEndPointFetcher m_mediatorEndpointFetcher;
    boost::optional< SocketAddress > m_mediatorAddress;
};

}   //cc
}   //nx

#endif // NX_CC_CLOUD_CONNECTION_INFO_H
