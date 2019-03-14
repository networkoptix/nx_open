#include "outgoing_tunnel_connection.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

OutgoingTunnelConnection::OutgoingTunnelConnection(
    aio::AbstractAioThread* aioThread,
    std::string connectionId,
    std::unique_ptr<UdtStreamSocket> udtConnection,
    Timeouts timeouts)
:
    AbstractOutgoingTunnelConnection(aioThread),
    m_connectionId(std::move(connectionId)),
    m_localPunchedAddress(udtConnection->getLocalAddress()),
    m_remoteHostAddress(udtConnection->getForeignAddress()),
    m_controlConnection(std::make_unique<ConnectionType>(std::move(udtConnection))),
    m_timeouts(timeouts),
    m_pleaseStopHasBeenCalled(false),
    m_pleaseStopCompleted(false)
{
    m_controlConnection->registerCloseHandler(
        [this](auto reason) { onConnectionClosed(reason); });

    m_controlConnection->bindToAioThread(getAioThread());
    std::chrono::milliseconds timeout = m_timeouts.maxConnectionInactivityPeriod();

    m_controlConnection->socket()->setNonBlockingMode(true);
    m_controlConnection->socket()->setRecvTimeout(timeout.count());
    m_controlConnection->setMessageHandler(
        std::bind(&OutgoingTunnelConnection::onStunMessageReceived,
            this, std::placeholders::_1));
}

OutgoingTunnelConnection::OutgoingTunnelConnection(
    aio::AbstractAioThread* aioThread,
    std::string connectionId,
    std::unique_ptr<UdtStreamSocket> udtConnection)
:
    OutgoingTunnelConnection(
        aioThread,
        std::move(connectionId),
        std::move(udtConnection),
        Timeouts())
{
}

OutgoingTunnelConnection::~OutgoingTunnelConnection()
{
    //all internal sockets live in same aio thread,
        //so it is allowed to free OutgoingTunnelConnection while in aio thread
    stopWhileInAioThread();
}

void OutgoingTunnelConnection::stopWhileInAioThread()
{
    //caller MUST guarantee that no calls to establishNewConnection can follow
    //and establishNewConnection has returned
    m_pleaseStopHasBeenCalled = true;

    //cancelling ongoing connects
    m_ongoingConnections.clear();
    m_controlConnection.reset();

    m_pleaseStopCompleted = true;
}

void OutgoingTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractOutgoingTunnelConnection::bindToAioThread(aioThread);
    m_controlConnection->bindToAioThread(aioThread);
}

void OutgoingTunnelConnection::start()
{
    hpm::api::UdpHolePunchingSynRequest syn;
    stun::Message message;
    syn.serialize(&message);
    m_controlConnection->sendMessage(std::move(message));

    m_controlConnection->startReadingConnection();
}

void OutgoingTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    NX_ASSERT(!m_pleaseStopHasBeenCalled);

    NX_VERBOSE(this, lm("cross-nat %1. New stream socket has been requested")
        .arg(m_connectionId));

    auto newConnection = std::make_unique<UdtStreamSocket>(SocketFactory::udpIpVersion());
    if (!socketAttributes.applyTo(newConnection.get()) ||
        !newConnection->bind(SocketAddress(HostAddress::anyHost, m_localPunchedAddress.port)) ||
        !newConnection->setNonBlockingMode(true))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_ASSERT(errorCode != SystemError::noError);
        NX_DEBUG(this, lm("cross-nat %1. Failed to apply socket options to new connection. %2")
            .arg(m_connectionId).arg(SystemError::toString(errorCode)));
        post(
            [this, handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr, m_controlConnection != nullptr);
            });
        return;
    }

    //temporariliy binding new socket to the same aio thread to simplify code here
    newConnection->bindToAioThread(getAioThread());

    QnMutexLocker lk(&m_mutex);

    auto connectionPtr = newConnection.get();
    m_ongoingConnections.emplace(
        connectionPtr,
        ConnectionContext{
            std::move(newConnection),
            std::move(handler),
            socketAttributes.aioThread ? *socketAttributes.aioThread : nullptr});

    //has to start connect and timer atomically,
        //but this can be done from socket's aio thread only
        //So, switching to newConnection's aio thread
    connectionPtr->post(
        std::bind(
            &OutgoingTunnelConnection::proceedWithConnection, this,
            connectionPtr, timeout));

    NX_ASSERT(!m_pleaseStopHasBeenCalled);
}

void OutgoingTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_controlConnectionClosedHandler = std::move(handler);
}

std::string OutgoingTunnelConnection::toString() const
{
    return lm("UDP hole punching from %1 to %2")
        .args(m_localPunchedAddress, m_remoteHostAddress).toStdString();
}

void OutgoingTunnelConnection::proceedWithConnection(
    UdtStreamSocket* connectionPtr,
    std::chrono::milliseconds timeout)
{
    NX_VERBOSE(this, lm("cross-nat %1. timeout %2")
        .arg(m_connectionId).arg(timeout));

    //we are in connectionPtr socket aio thread

    //if we are in this method, then connectionPtr is certainly alive
    QnMutexLocker lk(&m_mutex);
    //checking that connection has not been cancelled by pleaseStop
    if (m_ongoingConnections.find(connectionPtr) == m_ongoingConnections.end())
        return; //connection has been cancelled by pleaseStop, ignoring...

    NX_VERBOSE(this, lm("cross-nat %1. Initiating async connect to %2 with timeout %3")
        .arg(m_connectionId).arg(m_remoteHostAddress.toString()).arg(timeout));

    const bool timoutPresent = timeout > std::chrono::milliseconds::zero();

    connectionPtr->connectAsync(
        m_remoteHostAddress,
        [this, connectionPtr, timoutPresent](
            SystemError::ErrorCode errorCode)
        {
            //cancelling timer
            if (timoutPresent)
                connectionPtr->cancelIOSync(aio::etTimedOut);

            onConnectCompleted(connectionPtr, errorCode);
        });

    if (timoutPresent)
        connectionPtr->registerTimer(
            timeout,
            [connectionPtr, this]
            {
                connectionPtr->cancelIOSync(aio::etNone);   //cancelling connect
                onConnectCompleted(connectionPtr, SystemError::timedOut);
            });
}

void OutgoingTunnelConnection::onConnectCompleted(
    UdtStreamSocket* connectionPtr,
    SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, lm("cross-nat %1. result: %2")
        .arg(m_connectionId).arg(SystemError::toString(errorCode)));

    //called from connectionPtr completion handler

    //ensuring connectionPtr can be safely freed in any thread
    connectionPtr->post(
        std::bind(
            &OutgoingTunnelConnection::reportConnectResult, this,
            connectionPtr, errorCode));
}

void OutgoingTunnelConnection::reportConnectResult(
    UdtStreamSocket* connectionPtr,
    SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, lm("cross-nat %1. result: %2")
        .arg(m_connectionId).arg(SystemError::toString(errorCode)));

    //we are in object's thread

    QnMutexLocker lk(&m_mutex);
    auto connectionIter = m_ongoingConnections.find(connectionPtr);
    if (connectionIter == m_ongoingConnections.end())
        return; //this can happen after OutgoingTunnelConnection::pleaseStop has been called

    ConnectionContext connectionContext = std::move(connectionIter->second);
    m_ongoingConnections.erase(connectionIter);
    lk.unlock();

    if (errorCode == SystemError::noError)
    {
        //binding to desired aio thread
        connectionContext.connection->bindToAioThread(
            connectionContext.aioThreadToUse
                ? connectionContext.aioThreadToUse
                : nx::network::SocketGlobals::aioService().getRandomAioThread());
    }

    connectionContext.completionHandler(
        errorCode,
        errorCode == SystemError::noError
            ? std::move(connectionContext.connection)
            : std::unique_ptr<AbstractStreamSocket>(),
        m_controlConnection != nullptr);
}

void OutgoingTunnelConnection::onConnectionClosed(
    SystemError::ErrorCode closeReason)
{
    NX_DEBUG(this, lm("cross-nat %1. Control connection has been closed: %2")
        .arg(m_connectionId).arg(SystemError::toString(closeReason)));

    auto controlConnection = std::move(m_controlConnection);

    if (m_controlConnectionClosedHandler)
    {
        auto controlConnectionClosedHandler =
            std::move(m_controlConnectionClosedHandler);
        controlConnectionClosedHandler(closeReason);
    }
}

void OutgoingTunnelConnection::onStunMessageReceived(
    nx::network::stun::Message message)
{
    hpm::api::UdpHolePunchingSynResponse synAck;
    if (synAck.parse(message))
    {
        if (synAck.connectSessionId != m_connectionId)
        {
            NX_DEBUG(this, lm("cross-nat %1. Received SYN response with unexpected "
                       "connection id: %2 vs %1")
                .arg(m_connectionId).arg(synAck.connectSessionId));
        }
    }
    else
    {
        NX_DEBUG(this, lm("cross-nat %1. Failed to parse SYN response")
            .arg(m_connectionId));
    }

    NX_VERBOSE(this, lm("cross-nat %1. Control connection has been verified")
        .arg(m_connectionId));
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
