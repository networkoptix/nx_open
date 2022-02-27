// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include "abstract_incoming_tunnel_connection.h"

#include <nx/network/cloud/mediator_server_connections.h>

namespace nx::network::cloud {

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

    /**
     * Common connection info setters.
     */

    void setConnectionInfo(std::string connectionId, std::string remotePeerId);
    void setMediatorConnection(hpm::api::MediatorServerTcpConnection* connection);

    std::string connectionId() const;
    std::string remotePeerId() const;

    /**
     * Shall be called only once to estabilish incomming tunnel connection.
     */
    virtual void accept(AcceptHandler handler) = 0;

    virtual std::string toString() const = 0;

protected:
    hpm::api::MediatorServerTcpConnection* m_mediatorConnection = nullptr;
    std::string m_connectionId;
    std::string m_remotePeerId;
};

} // namespace nx::network::cloud
