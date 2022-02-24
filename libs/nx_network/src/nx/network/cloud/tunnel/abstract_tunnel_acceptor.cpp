// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_tunnel_acceptor.h"

namespace nx::network::cloud {

void AbstractTunnelAcceptor::setConnectionInfo(
    std::string connectionId, std::string remotePeerId)
{
    m_connectionId = std::move(connectionId);
    m_remotePeerId = std::move(remotePeerId);
}

void AbstractTunnelAcceptor::setMediatorConnection(
    hpm::api::MediatorServerTcpConnection*  connection)
{
    m_mediatorConnection = connection;
}

std::string AbstractTunnelAcceptor::connectionId() const
{
    return m_connectionId;
}

std::string AbstractTunnelAcceptor::remotePeerId() const
{
    return m_remotePeerId;
}

} // namespace nx::network::cloud
