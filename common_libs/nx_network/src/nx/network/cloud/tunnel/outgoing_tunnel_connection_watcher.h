#pragma once

#include <nx/network/aio/timer.h>

#include "abstract_outgoing_tunnel_connection.h"
#include "../data/connection_parameters.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Closes tunnel if not used for some time.
 */
class NX_NETWORK_API OutgoingTunnelConnectionWatcher:
    public AbstractOutgoingTunnelConnection
{
    typedef AbstractOutgoingTunnelConnection BaseType;

public:
    OutgoingTunnelConnectionWatcher(
        nx::hpm::api::ConnectionParameters connectionParameters,
        std::unique_ptr<AbstractOutgoingTunnelConnection> tunnelConnection);
    virtual ~OutgoingTunnelConnectionWatcher();

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
    const nx::hpm::api::ConnectionParameters m_connectionParameters;
    std::unique_ptr<AbstractOutgoingTunnelConnection> m_tunnelConnection;
    std::unique_ptr<aio::Timer> m_inactivityTimer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onTunnelClosedHandler;
    SystemError::ErrorCode m_statusCode = SystemError::noError;

    void launchInactivityTimer();
    void closeTunnel(SystemError::ErrorCode reason);
};

} // namespace cloud
} // namespace network
} // namespace nx
