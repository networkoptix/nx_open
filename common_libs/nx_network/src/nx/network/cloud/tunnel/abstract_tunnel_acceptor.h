#pragma once

#include "abstract_incoming_tunnel_connection.h"

#include <nx/network/cloud/mediator_connections.h>

namespace nx {
namespace network {
namespace cloud {

/**
 *  Creates incoming specialized connected AbstractIncomingTunnelConnection
 *  using one of a nat traversal methods.
 */
class NX_NETWORK_API AbstractTunnelAcceptor
:
    public QnStoppableAsync
{
public:
    // common connection info setters
    void setConnectionInfo(String connectionId, String remotePeerId);
    void setMediatorConnection(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> connection);

    /** Shell be called only once to estabilish incomming tunnel connection */
    virtual void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)> handler) = 0;

protected:
    std::shared_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    String m_connectionId;
    String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
