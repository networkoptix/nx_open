#pragma once

#include <map>

#include <nx/network/socket.h>
#include <nx/utils/thread/mutex.h>

#include "endpoint_selector.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Selects any endpoint that accepts tcp connections.
 */
class NX_NETWORK_API RandomOnlineEndpointSelector:
    public AbstractEndpointSelector
    // TODO: #ak Inherit from aio::BasicPollable
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

} // namespace cloud
} // namespace network
} // namespace nx
