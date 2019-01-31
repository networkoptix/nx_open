#pragma once

#include <chrono>

#include <nx/network/aio/abstract_async_connector.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/std/optional.h>

#include "listening_peer_pool.h"

namespace nx {
namespace cloud {
namespace relaying {

class NX_RELAYING_API ListeningPeerConnector:
    public nx::network::aio::AbstractAsyncConnector
{
    using base_type = nx::network::aio::AbstractAsyncConnector;

public:
    using SuccessfulConnectHandler =
        nx::utils::MoveOnlyFunc<void(const std::string& /*targetHostName*/)>;

    ListeningPeerConnector(relaying::AbstractListeningPeerPool* listeningPeerPool);
    ~ListeningPeerConnector();

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connectAsync(
        const network::SocketAddress& targetEndpoint,
        ConnectHandler handler) override;

    void setTimeout(std::chrono::milliseconds timeout);

    void setOnSuccessfulConnect(SuccessfulConnectHandler handler);

private:
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;
    network::SocketAddress m_targetEndpoint;
    ConnectHandler m_completionHandler;
    nx::utils::AsyncOperationGuard m_guard;
    std::optional<std::chrono::milliseconds> m_timeout;
    network::aio::Timer m_timer;
    bool m_cancelled = false;
    SuccessfulConnectHandler m_successulConnectHandler;

    void connectInAioThread();
    void cancelTakeIdleConnection();
    void reportFailure(SystemError::ErrorCode systemErrorCode);

    void processTakeConnectionResult(
        cloud::relay::api::ResultCode resultCode,
        std::unique_ptr<network::AbstractStreamSocket> connection,
        const std::string& peerName);
};

} // namespace relaying
} // namespace cloud
} // namespace nx
