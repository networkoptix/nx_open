/**********************************************************
* Oct 7, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CC_CDB_RANDOM_ONLINE_ENDPOINT_SELECTOR_H
#define NX_CC_CDB_RANDOM_ONLINE_ENDPOINT_SELECTOR_H

#include <map>

#include <nx/network/socket.h>
#include <nx/tool/thread/mutex.h>

#include "endpoint_selector.h"


namespace nx {
namespace cc {

//!Selects any endpoint that accepts tcp connections
class NX_NETWORK_API RandomOnlineEndpointSelector
:
    public AbstractEndpointSelector
{
public:
    RandomOnlineEndpointSelector();
    virtual ~RandomOnlineEndpointSelector();

    virtual void selectBestEndpont(
        const QString& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler) override;

private:
    void done(
        AbstractStreamSocket* sock,
        SystemError::ErrorCode errorCode,
        SocketAddress endpoint);
    
    std::function<void(nx_http::StatusCode::Value, SocketAddress)> m_handler;
    bool m_endpointResolved;
    std::map<AbstractStreamSocket*, std::unique_ptr<AbstractStreamSocket>> m_sockets;
    size_t m_socketsStillConnecting;
    mutable QnMutex m_mutex;
};

}   //cc
}   //nx

#endif  //NX_CC_CDB_RANDOM_ONLINE_ENDPOINT_SELECTOR_H
