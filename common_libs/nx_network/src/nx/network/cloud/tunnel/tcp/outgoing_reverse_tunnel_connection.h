#pragma once

#include <nx/network/cloud/tunnel/tcp/reverse_connection_pool.h>
#include <nx/network/cloud/tunnel/abstract_outgoing_tunnel_connection.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

class OutgoingReverseTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
public:
    OutgoingReverseTunnelConnection(
        aio::AbstractAioThread* aioThread,
        std::shared_ptr<ReverseConnectionHolder> connectionHolder);

    void stopWhileInAioThread() override;

    void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;

    void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

private:
    void updateCloseTimer();

    const std::shared_ptr<ReverseConnectionHolder> m_connectionHolder;
    utils::AsyncOperationGuard m_asyncGuard;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_closedHandler;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
