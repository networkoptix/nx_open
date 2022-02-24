// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/cloud/tunnel/abstract_incoming_tunnel_connection.h>
#include <nx/network/cloud/tunnel/udp/incoming_control_connection.h>
#include <nx/network/udt/udt_socket.h>

namespace nx::network::cloud::udp {

/**
 * Creates UDT server socket on the IncomingControlConnection's port to accept direct connections
 * while control connection is alive. The error is reported by accept handler as soon as control
 * connection dies.
 */
class NX_NETWORK_API IncomingTunnelConnection:
    public AbstractIncomingTunnelConnection
{
    using base_type = AbstractIncomingTunnelConnection;

public:
    IncomingTunnelConnection(
        std::unique_ptr<IncomingControlConnection> controlConnection,
        std::unique_ptr<AbstractStreamServerSocket> serverSocket = nullptr);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void accept(AcceptHandler handler) override;

private:
    SystemError::ErrorCode m_state;
    std::unique_ptr<IncomingControlConnection> m_controlConnection;
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    AcceptHandler m_acceptHandler;

    virtual void stopWhileInAioThread() override;
};

} // namespace nx::network::cloud::udp
