// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "direct_endpoint_tunnel.h"

#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

static const nx::network::KeepAliveOptions kControlConnectionKeepAlive(
    std::chrono::minutes(1), std::chrono::seconds(10), 3);

namespace nx::network::cloud::tcp {

DirectTcpEndpointTunnel::DirectTcpEndpointTunnel(
    aio::AbstractAioThread* aioThread,
    std::string connectSessionId,
    SocketAddress targetEndpoint,
    std::unique_ptr<AbstractStreamSocket> /*connection*/)
    :
    AbstractOutgoingTunnelConnection(aioThread),
    m_connectSessionId(std::move(connectSessionId)),
    m_targetEndpoint(std::move(targetEndpoint)),
    m_targetEndpointIpVersion(
        m_targetEndpoint.address.isPureIpV6()
        ? AF_INET6
        : SocketFactory::tcpClientIpVersion())/*,
    m_controlConnection(std::move(connection))*/
{
    if (aioThread && m_controlConnection)
        m_controlConnection->bindToAioThread(aioThread);
}

DirectTcpEndpointTunnel::~DirectTcpEndpointTunnel()
{
    stopWhileInAioThread();
}

void DirectTcpEndpointTunnel::stopWhileInAioThread()
{
    m_controlConnection.reset();

    auto connectionClosedHandler = std::move(m_connectionClosedHandler);
    m_connectionClosedHandler = nullptr;

    m_connections.clear();

    if (connectionClosedHandler)
        connectionClosedHandler(SystemError::interrupted);
}

void DirectTcpEndpointTunnel::start()
{
    NX_VERBOSE(this, "cross-nat %1. Starting TCP tunnel to %2 (ip v%3) %4 control connection",
        m_connectSessionId, m_targetEndpoint, m_targetEndpointIpVersion,
        m_controlConnection ? "with" : "without");

    if (!m_controlConnection)
        return;

    const auto buffer = std::make_shared<Buffer>();
    buffer->reserve(1);
    NX_ASSERT(m_controlConnection->setKeepAlive(kControlConnectionKeepAlive));
    m_controlConnection->readSomeAsync(
        buffer.get(),
        [this, buffer](SystemError::ErrorCode code, size_t bytesRead)
        {
            if (code == SystemError::noError && bytesRead == 0)
            {
                NX_DEBUG(this, "cross-nat %1. Control connection has been closed by remote peer",
                    m_connectSessionId);
            }
            else
            {
                NX_DEBUG(this, "cross-nat %1. Unexpected read event on control connection (size=%2): %3",
                    m_connectSessionId, bytesRead, SystemError::toString(code));
            }

            m_controlConnection.reset();
            if (m_connectionClosedHandler)
                utils::moveAndCall(m_connectionClosedHandler, SystemError::connectionReset);
        });
}

void DirectTcpEndpointTunnel::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    ConnectionContext context{
        std::move(socketAttributes),
        std::move(handler),
        nullptr };

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_connections.push_back(std::move(context));
    auto it = --m_connections.end();

    post(std::bind(&DirectTcpEndpointTunnel::startConnection, this, it, timeout));
}

void DirectTcpEndpointTunnel::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_connectionClosedHandler = std::move(handler);
}

ConnectType DirectTcpEndpointTunnel::connectType() const
{
    return ConnectType::forwardedTcpPort;
}

std::string DirectTcpEndpointTunnel::toString() const
{
    return nx::format("Direct tcp connect to %1").args(m_targetEndpoint).toStdString();
}

void DirectTcpEndpointTunnel::startConnection(
    std::list<ConnectionContext>::iterator connectionContextIter,
    std::chrono::milliseconds timeout)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "cross-nat %1. Opening new connection", m_connectSessionId);

    connectionContextIter->tcpSocket =
        std::make_unique<TCPSocket>(m_targetEndpointIpVersion);
    connectionContextIter->tcpSocket->bindToAioThread(getAioThread());
    if (!connectionContextIter->tcpSocket->setNonBlockingMode(true) ||
        !connectionContextIter->tcpSocket->setSendTimeout(timeout.count()))
    {
        reportConnectResult(
            connectionContextIter,
            SystemError::getLastOSErrorCode(),
            nullptr,
            true);
        return;
    }

    connectionContextIter->tcpSocket->connectAsync(
        m_targetEndpoint,
        std::bind(&DirectTcpEndpointTunnel::onConnectDone, this, _1, connectionContextIter));
}

void DirectTcpEndpointTunnel::onConnectDone(
    SystemError::ErrorCode sysErrorCode,
    std::list<ConnectionContext>::iterator connectionContextIter)
{
    connectionContextIter->tcpSocket->post(
        [this, sysErrorCode, connectionContextIter]()
        {
            if (sysErrorCode != SystemError::noError)
                connectionContextIter->tcpSocket.reset();

            reportConnectResult(
                connectionContextIter,
                sysErrorCode,
                std::move(connectionContextIter->tcpSocket),
                sysErrorCode == SystemError::noError);
        });
}

void DirectTcpEndpointTunnel::reportConnectResult(
    std::list<ConnectionContext>::iterator connectionContextIter,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> tcpSocket,
    bool stillValid)
{
    NX_VERBOSE(this, "cross-nat %1. New connection to %2 completed. %3. Tunnel valid: %4",
        m_connectSessionId, m_targetEndpoint, SystemError::toString(sysErrorCode), stillValid);

    auto context = std::move(*connectionContextIter);
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_connections.erase(connectionContextIter);
    }

    if (tcpSocket && !context.socketAttributes.applyTo(tcpSocket.get()))
    {
        sysErrorCode = SystemError::getLastOSErrorCode();
        stillValid = false;
        tcpSocket.reset();
    }

    nx::utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
    context.handler(sysErrorCode, std::move(tcpSocket), stillValid);
    if (watcher.interrupted())
        return;

    if (!stillValid && m_connectionClosedHandler)
    {
        auto connectionClosedHandler = std::move(m_connectionClosedHandler);
        m_connectionClosedHandler = nullptr;
        connectionClosedHandler(sysErrorCode);
    }
}

} // namespace nx::network::cloud::tcp
