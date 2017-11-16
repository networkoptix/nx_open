#include "direct_endpoint_tunnel.h"

#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

static const KeepAliveOptions kControlConnectionKeepAlive(
    std::chrono::minutes(1), std::chrono::seconds(10), 3);

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
    m_controlConnection(std::move(connection))
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
    NX_LOGX(lm("Start %1 control connection").arg(m_controlConnection ? "with" : "without"),
        cl_logDEBUG2);

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
                NX_LOGX(lm("Control connection has been closed by remote peer"), cl_logDEBUG1);
            }
            else
            {
                NX_LOGX(lm("Unexpected read event on control connection (size=%1): %2").args(
                    bytesRead, SystemError::toString(code)), cl_logDEBUG1);
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

std::string DirectTcpEndpointTunnel::toString() const
{
    return lm("Direct tcp connect to %1").args(m_targetEndpoint).toStdString();
}

void DirectTcpEndpointTunnel::startConnection(
    std::list<ConnectionContext>::iterator connectionContextIter,
    std::chrono::milliseconds timeout)
{
    connectionContextIter->tcpSocket =
        std::make_unique<TCPSocket>(SocketFactory::tcpClientIpVersion());
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

    using namespace std::placeholders;
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

    if (tcpSocket && !context.socketAttributes.applyTo(tcpSocket.get()))
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
