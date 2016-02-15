/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#include "outgoing_tunnel_udt_connection.h"


namespace nx {
namespace network {
namespace cloud {

OutgoingTunnelUdtConnection::OutgoingTunnelUdtConnection(
    std::unique_ptr<UdtStreamSocket> udtConnection,
    SocketAddress targetPeerAddress)
{
}

void OutgoingTunnelUdtConnection::pleaseStop(
    std::function<void()> completionHandler)
{
    //TODO #ak
}

void OutgoingTunnelUdtConnection::establishNewConnection(
    boost::optional<std::chrono::milliseconds> timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    //TODO #ak
}

} // namespace cloud
} // namespace network
} // namespace nx
