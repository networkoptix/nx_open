#pragma once

#include <chrono>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace gateway {

/**
 * First, tries to find connection in ListeningPeerPool.
 * If not succeeded, uses regular (cloud) connection.
 */
class NX_VMS_GATEWAY_API TargetPeerConnector:
    public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;

public:
    using ConnectHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode /*systemErrorCode*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connectionToTheTargetPeer*/)>;

    TargetPeerConnector(
        relaying::AbstractListeningPeerPool* listeningPeerPool,
        const network::SocketAddress& targetEndpoint);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    void setTimeout(std::chrono::milliseconds timeout);
    void connectAsync(ConnectHandler handler);

private:
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;
    const network::SocketAddress m_targetEndpoint;
    boost::optional<std::chrono::milliseconds> m_timeout;
    ConnectHandler m_completionHandler;
    std::unique_ptr<network::AbstractStreamSocket> m_targetPeerSocket;
    nx::network::aio::Timer m_timer;

    virtual void stopWhileInAioThread() override;

    void takeConnectionFromListeningPeerPool();
    void processTakeConnectionResult(
        cloud::relay::api::ResultCode resultCode,
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
