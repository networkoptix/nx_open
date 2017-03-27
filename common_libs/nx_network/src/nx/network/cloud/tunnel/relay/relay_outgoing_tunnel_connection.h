#pragma once

#include "../abstract_outgoing_tunnel_connection.h"
#include "api/client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API OutgoingTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
public:
    OutgoingTunnelConnection(
        SocketAddress relayEndpoint,
        nx::String relaySessionId,
        std::unique_ptr<api::ClientToRelayConnection> clientToRelayConnection);
    virtual ~OutgoingTunnelConnection();

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;
    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

private:
    const SocketAddress m_relayEndpoint;
    const nx::String m_relaySessionId;
    std::unique_ptr<api::ClientToRelayConnection> m_clientToRelayConnection;
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
