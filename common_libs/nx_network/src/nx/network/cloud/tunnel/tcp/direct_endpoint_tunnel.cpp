/**********************************************************
* Jul 25, 2016
* akolesnikov
***********************************************************/

#include "direct_endpoint_tunnel.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>


namespace nx {
namespace network {
namespace cloud {
namespace tcp {

DirectTcpEndpointTunnel::DirectTcpEndpointTunnel(
    aio::AbstractAioThread* aioThread,
    nx::String connectSessionId,
    SocketAddress targetEndpoint,
    std::unique_ptr<AbstractStreamSocket> connection)
    :
    AbstractOutgoingTunnelConnection(aioThread),
    m_connectSessionId(std::move(connectSessionId)),
    m_targetEndpoint(std::move(targetEndpoint)),
    m_tcpConnection(std::move(connection))
{
    if (m_tcpConnection && aioThread)
        m_tcpConnection->bindToAioThread(aioThread);
}

DirectTcpEndpointTunnel::~DirectTcpEndpointTunnel()
{
    stopWhileInAioThread();
}

void DirectTcpEndpointTunnel::stopWhileInAioThread()
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

void DirectTcpEndpointTunnel::establishNewConnection(
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

    post(std::bind(&DirectTcpEndpointTunnel::startConnection, this, it, timeout));
}

void DirectTcpEndpointTunnel::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_connectionClosedHandler = std::move(handler);
}

void DirectTcpEndpointTunnel::startConnection(
    std::list<ConnectionContext>::iterator connectionContextIter,
    std::chrono::milliseconds timeout)
{
    using namespace std::placeholders;

    if (m_tcpConnection)
    {
        auto tcpConnection = std::move(m_tcpConnection);
        m_tcpConnection = nullptr;
        SystemError::ErrorCode sysErrorCodeToReport = SystemError::noError;
        if (!connectionContextIter->socketAttributes.applyTo(tcpConnection.get()))
        {
            sysErrorCodeToReport = SystemError::getLastOSErrorCode();
            tcpConnection.reset();
        }
        reportConnectResult(
            connectionContextIter,
            sysErrorCodeToReport,
            std::move(tcpConnection),
            true);
        return;
    }

    connectionContextIter->tcpSocket = std::make_unique<TCPSocket>(AF_INET);
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
    auto context = std::move(*connectionContextIter);
    {
        QnMutexLocker lk(&m_mutex);
        m_connections.erase(connectionContextIter);
    }
    
    if (!context.socketAttributes.applyTo(tcpSocket.get()))
    {
        sysErrorCode = SystemError::getLastOSErrorCode();
        stillValid = false;
        tcpSocket.reset();
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
