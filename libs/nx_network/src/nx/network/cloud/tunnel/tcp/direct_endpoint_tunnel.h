// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/interruption_flag.h>
#include <nx/utils/thread/mutex.h>

#include "../abstract_outgoing_tunnel_connection.h"

namespace nx::network::cloud::tcp {

class NX_NETWORK_API DirectTcpEndpointTunnel:
    public AbstractOutgoingTunnelConnection
{
public:
    DirectTcpEndpointTunnel(
        aio::AbstractAioThread* aioThread,
        std::string connectSessionId,
        SocketAddress targetEndpoint,
        std::unique_ptr<AbstractStreamSocket> connection);

    virtual ~DirectTcpEndpointTunnel();

    virtual void stopWhileInAioThread() override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual ConnectType connectType() const override;

    virtual std::string toString() const override;

private:
    struct ConnectionContext
    {
        SocketAttributes socketAttributes;
        OnNewConnectionHandler handler;
        std::unique_ptr<AbstractStreamSocket> tcpSocket;
    };

    const std::string m_connectSessionId;
    const SocketAddress m_targetEndpoint;
    const int m_targetEndpointIpVersion;
    std::unique_ptr<AbstractStreamSocket> m_controlConnection;
    std::list<ConnectionContext> m_connections;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectionClosedHandler;
    mutable nx::Mutex m_mutex;
    nx::utils::InterruptionFlag m_destructionFlag;

    void startConnection(
        std::list<ConnectionContext>::iterator connectionContextIter,
        std::chrono::milliseconds timeout);
    void onConnectDone(
        SystemError::ErrorCode sysErrorCode,
        std::list<ConnectionContext>::iterator connectionContextIter);
    void reportConnectResult(
        std::list<ConnectionContext>::iterator connectionContextIter,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> tcpSocket,
        bool stillValid);
};

} // namespace nx::network::cloud::tcp
