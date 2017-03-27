#include "relay_outgoing_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

OutgoingTunnelConnection::OutgoingTunnelConnection(
    SocketAddress relayEndpoint,
    nx::String relaySessionId,
    std::unique_ptr<api::ClientToRelayConnection> clientToRelayConnection)
    :
    m_relayEndpoint(std::move(relayEndpoint)),
    m_relaySessionId(std::move(relaySessionId)),
    m_clientToRelayConnection(std::move(clientToRelayConnection))
{
}

OutgoingTunnelConnection::~OutgoingTunnelConnection()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void OutgoingTunnelConnection::stopWhileInAioThread()
{
}

void OutgoingTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
}

void OutgoingTunnelConnection::start()
{
}

void OutgoingTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
}

void OutgoingTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
