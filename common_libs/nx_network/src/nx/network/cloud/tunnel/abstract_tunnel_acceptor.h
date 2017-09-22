#pragma once

#include <string>

#include "abstract_incoming_tunnel_connection.h"

#include <nx/network/cloud/mediator_server_connections.h>

namespace nx {
namespace network {
namespace cloud {

/**
 * Creates incoming specialized connected AbstractIncomingTunnelConnection
 * using one of a nat traversal methods.
 */
class NX_NETWORK_API AbstractTunnelAcceptor:
    public QnStoppableAsync
{
public:
    using AcceptHandler = std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractIncomingTunnelConnection>)>;

    AbstractTunnelAcceptor();

    /**
     * Common connection info setters.
     */
    
    void setConnectionInfo(String connectionId, String remotePeerId);
    void setMediatorConnection(hpm::api::MediatorServerTcpConnection* connection);

    String connectionId() const;
    String remotePeerId() const;

    /**
     * Shall be called only once to estabilish incomming tunnel connection.
     */
    virtual void accept(AcceptHandler handler) = 0;

    virtual std::string toString() const = 0;

protected:
    hpm::api::MediatorServerTcpConnection* m_mediatorConnection;
    String m_connectionId;
    String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
