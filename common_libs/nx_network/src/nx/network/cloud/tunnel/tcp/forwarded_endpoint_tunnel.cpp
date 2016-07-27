/**********************************************************
* Jul 25, 2016
* akolesnikov
***********************************************************/

#include "forwarded_endpoint_tunnel.h"

#include <nx/utils/log/log.h>


namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ForwardedTcpEndpointTunnel::ForwardedTcpEndpointTunnel(
    nx::String connectSessionId,
    SocketAddress targetEndpoint,
    std::unique_ptr<TCPSocket> connection)
    :
    m_connectSessionId(std::move(connectSessionId)),
    m_targetEndpoint(std::move(targetEndpoint)),
    m_tcpConnection(std::move(connection))
{
}

ForwardedTcpEndpointTunnel::~ForwardedTcpEndpointTunnel()
{
    stopWhileInAioThread();
}

void ForwardedTcpEndpointTunnel::stopWhileInAioThread()
{
    m_tcpConnection.reset();
    
    auto connectionClosedHandler = std::move(m_connectionClosedHandler);
    m_connectionClosedHandler = nullptr;

    auto connections = std::move(m_connections);
    for (auto& connectionContext : connections)
    {
        connectionContext.handler(
            SystemError::interrupted,
            nullptr,
            false);
    }

    if (connectionClosedHandler)
        connectionClosedHandler(SystemError::interrupted);
}

void ForwardedTcpEndpointTunnel::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    ConnectionContext context{
        std::move(socketAttributes),
        std::move(handler),
        nullptr };

    QnMutexLocker lk(&m_mutex);
    m_connections.push_back(std::move(context));
    auto it = --m_connections.end();

    post(std::bind(&ForwardedTcpEndpointTunnel::startConnection, this, it, timeout));
}

void ForwardedTcpEndpointTunnel::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_connectionClosedHandler = std::move(handler);
}

void ForwardedTcpEndpointTunnel::startConnection(
    std::list<ConnectionContext>::iterator connectionContextIter,
    std::chrono::milliseconds timeout)
{
    using namespace std::placeholders;

    if (m_tcpConnection)
    {
        auto tcpConnection = std::move(m_tcpConnection);
        m_tcpConnection = nullptr;
        reportConnectResult(
            connectionContextIter,
            SystemError::noError,
            std::move(tcpConnection),
            true);
        return;
    }

    connectionContextIter->tcpSocket = std::make_unique<TCPSocket>();
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
        std::bind(&ForwardedTcpEndpointTunnel::onConnectDone, this, _1, connectionContextIter));
}

void ForwardedTcpEndpointTunnel::onConnectDone(
    SystemError::ErrorCode sysErrorCode,
    std::list<ConnectionContext>::iterator connectionContextIter)
{
    reportConnectResult(
        connectionContextIter,
        sysErrorCode,
        std::move(connectionContextIter->tcpSocket),
        sysErrorCode == SystemError::noError);
}

void ForwardedTcpEndpointTunnel::reportConnectResult(
    std::list<ConnectionContext>::iterator connectionContextIter,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<TCPSocket> tcpSocket,
    bool stillValid)
{
    auto context = std::move(*connectionContextIter);
    {
        QnMutexLocker lk(&m_mutex);
        m_connections.erase(connectionContextIter);
    }
    
    nx::utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
    context.handler(sysErrorCode, std::move(tcpSocket), stillValid);
    if (watcher.objectDestroyed())
        return;

    if (!stillValid && m_connectionClosedHandler)
    {
        auto connectionClosedHandler = std::move(m_connectionClosedHandler);
        m_connectionClosedHandler = nullptr;
        connectionClosedHandler(sysErrorCode);
    }
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
