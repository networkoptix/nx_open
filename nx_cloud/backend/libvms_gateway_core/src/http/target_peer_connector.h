#pragma once

#include <chrono>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/optional.h>

#include <nx/cloud/relaying/listening_peer_pool.h>
#include <nx/cloud/relaying/listening_peer_connector.h>

namespace nx {
namespace cloud {
namespace gateway {

/**
 * First, tries to find connection in ListeningPeerPool.
 * If not succeeded, uses regular (cloud) connection.
 */
class NX_VMS_GATEWAY_API TargetPeerConnector:
    public nx::network::aio::AbstractAsyncConnector
{
    using base_type = nx::network::aio::AbstractAsyncConnector;

public:
    TargetPeerConnector(
        relaying::AbstractListeningPeerPool* listeningPeerPool);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connectAsync(
        const network::SocketAddress& targetEndpoint,
        ConnectHandler handler) override;

    void setTimeout(std::chrono::milliseconds timeout);
    void setTargetConnectionRecvTimeout(std::optional<std::chrono::milliseconds> value);
    void setTargetConnectionSendTimeout(std::optional<std::chrono::milliseconds> value);

private:
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;
    network::SocketAddress m_targetEndpoint;
    boost::optional<std::chrono::milliseconds> m_timeout;
    ConnectHandler m_completionHandler;
    std::unique_ptr<network::AbstractStreamSocket> m_targetPeerSocket;
    nx::network::aio::Timer m_timer;
    std::optional<std::chrono::milliseconds> m_targetConnectionRecvTimeout;
    std::optional<std::chrono::milliseconds> m_targetConnectionSendTimeout;
    std::unique_ptr<relaying::ListeningPeerConnector> m_relayConnector;

    virtual void stopWhileInAioThread() override;

    void takeConnectionFromListeningPeerPool();
    void processTakeConnectionResult(
        SystemError::ErrorCode resultCode,
        std::unique_ptr<network::AbstractStreamSocket> connection);

    void initiateDirectConnection();
    void processDirectConnectionResult(SystemError::ErrorCode systemErrorCode);
    void processConnectionResult(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<network::AbstractStreamSocket> connection);
    void interruptByTimeout();
};

} // namespace gateway
} // namespace cloud
} // namespace nx
