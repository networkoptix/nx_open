/**********************************************************
* Jul 25, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/object_destruction_flag.h>

#include "../abstract_outgoing_tunnel_connection.h"


namespace nx {
namespace network {
namespace cloud {
namespace tcp {

class NX_NETWORK_API DirectTcpEndpointTunnel
    :
    public AbstractOutgoingTunnelConnection
{
public:
    DirectTcpEndpointTunnel(
        nx::String connectSessionId,
        SocketAddress targetEndpoint,
        std::unique_ptr<TCPSocket> connection);
    virtual ~DirectTcpEndpointTunnel();

    virtual void stopWhileInAioThread() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;
    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

private:
    struct ConnectionContext
    {
        SocketAttributes socketAttributes;
        OnNewConnectionHandler handler;
        std::unique_ptr<TCPSocket> tcpSocket;
    };

    const nx::String m_connectSessionId;
    const SocketAddress m_targetEndpoint;
    std::unique_ptr<TCPSocket> m_tcpConnection;
    std::list<ConnectionContext> m_connections;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectionClosedHandler;
    mutable QnMutex m_mutex;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    void startConnection(
        std::list<ConnectionContext>::iterator connectionContextIter,
        std::chrono::milliseconds timeout);
    void onConnectDone(
        SystemError::ErrorCode sysErrorCode,
        std::list<ConnectionContext>::iterator connectionContextIter);
    void reportConnectResult(
        std::list<ConnectionContext>::iterator connectionContextIter,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<TCPSocket> tcpSocket,
        bool stillValid);
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
