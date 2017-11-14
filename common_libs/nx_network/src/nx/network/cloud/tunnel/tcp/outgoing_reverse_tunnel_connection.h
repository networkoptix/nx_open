#pragma once

#include <memory>

#include <nx/network/aio/timer.h>

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
        std::shared_ptr<ReverseConnectionSource> connectionHolder);

    ~OutgoingReverseTunnelConnection();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual std::string toString() const override;

private:
    void updateCloseTimer();

    const std::shared_ptr<ReverseConnectionSource> m_connectionHolder;
    utils::AsyncOperationGuard m_asyncGuard;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_closedHandler;
    std::unique_ptr<aio::Timer> m_timer;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
