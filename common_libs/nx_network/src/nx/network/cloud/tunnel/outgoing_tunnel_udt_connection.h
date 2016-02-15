/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <memory>

#include "abstract_outgoing_tunnel_connection.h"
#include "nx/network/udt/udt_socket.h"


namespace nx {
namespace network {
namespace cloud {

class OutgoingTunnelUdtConnection
:
    public AbstractOutgoingTunnelConnection
{
public:
    OutgoingTunnelUdtConnection(
        std::unique_ptr<UdtStreamSocket> udtConnection,
        SocketAddress targetPeerAddress);

    virtual void pleaseStop(std::function<void()> completionHandler) override;

    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;
};

} // namespace cloud
} // namespace network
} // namespace nx
