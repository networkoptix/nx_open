// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "outgoing_tunnel_connection.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/barrier_handler.h>

namespace nx::network::cloud::udp {

const KeepAliveOptions Settings::kDefaultKeepAliveOptions(
    std::chrono::seconds::zero(),
    std::chrono::seconds(11),
    5);

OutgoingTunnelConnection::OutgoingTunnelConnection(
    aio::AbstractAioThread* aioThread,
    std::string connectionId,
    std::unique_ptr<UdtStreamSocket> udtConnection,
    Settings settings)
:
    AbstractOutgoingTunnelConnection(aioThread),
    m_connectionId(std::move(connectionId)),
    m_localPunchedAddress(udtConnection->getLocalAddress()),
    m_remoteHostAddress(udtConnection->getForeignAddress()),
    m_controlConnection(std::make_unique<ConnectionType>(std::move(udtConnection))),
    m_settings(settings)
{
    m_controlConnection->registerCloseHandler(
        [this](auto reason, auto /*connectionDestroyed*/) { onConnectionClosed(reason); });

    bindToAioThread(getAioThread());

    m_controlConnection->socket()->setNonBlockingMode(true);

    const std::chrono::milliseconds recvTimeout = m_settings.verificationTimeout;
    m_controlConnection->socket()->setRecvTimeout(recvTimeout);
    m_controlConnection->setMessageHandler(
        [this](auto&&... args) { onStunMessageReceived(std::forward<decltype(args)>(args)...); });

    // NOTE: Have to do this since STUN parser in vms <= 4.2 crashes when receiving FINGERPRINT
    // from this class.
    m_controlConnection->serializer().setAlwaysAddFingerprint(false);
}

OutgoingTunnelConnection::~OutgoingTunnelConnection()
{
    // All internal sockets live in same aio thread.
    // So, it is allowed to free OutgoingTunnelConnection while in aio thread.
    stopWhileInAioThread();
}

void OutgoingTunnelConnection::stopWhileInAioThread()
{
    // Caller MUST guarantee that no calls to establishNewConnection can follow
    // and establishNewConnection has returned.
    m_pleaseStopHasBeenCalled = true;

    // Cancelling ongoing connects.
    m_ongoingConnections.clear();
    m_controlConnection.reset();
    m_keepAliveTimer.pleaseStopSync();

    m_pleaseStopCompleted = true;
}

void OutgoingTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_controlConnection->bindToAioThread(aioThread);
    m_keepAliveTimer.bindToAioThread(aioThread);
}

void OutgoingTunnelConnection::start()
{
    NX_VERBOSE(this, "%1. Starting connection verification", m_connectionId);

    sendKeepAliveProbe();

    m_controlConnection->startReadingConnection();
}

void OutgoingTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    NX_ASSERT(!m_pleaseStopHasBeenCalled);

    NX_VERBOSE(this, "%1. New stream socket has been requested", m_connectionId);

    auto newConnection = std::make_unique<UdtStreamSocket>(SocketFactory::udpIpVersion());
    if (!socketAttributes.applyTo(newConnection.get()) ||
        !newConnection->bind(SocketAddress(HostAddress::anyHost, m_localPunchedAddress.port)) ||
        !newConnection->setNonBlockingMode(true))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_ASSERT(errorCode != SystemError::noError);
        NX_DEBUG(this, "%1. Failed to apply socket options to new connection. %2",
            m_connectionId, SystemError::toString(errorCode));
        post(
            [this, handler = std::move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr, m_controlConnection != nullptr);
            });
        return;
    }

    // Temporarily binding the new socket to the same aio thread to simplify the code here.
    newConnection->bindToAioThread(getAioThread());

    NX_MUTEX_LOCKER lk(&m_mutex);

    auto connectionPtr = newConnection.get();
    m_ongoingConnections.emplace(
        connectionPtr,
        ConnectionContext{
            std::move(newConnection),
            std::move(handler),
            socketAttributes.aioThread ? *socketAttributes.aioThread : nullptr});

    // We have to start connect and timer atomically, but this can be done from socket's aio
    // thread only. So, switching to newConnection's aio thread.
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

ConnectType OutgoingTunnelConnection::connectType() const
{
    return ConnectType::udpHp;
}

std::string OutgoingTunnelConnection::toString() const
{
    return nx::format("UDP hole punching from %1 to %2")
        .args(m_localPunchedAddress, m_remoteHostAddress).toStdString();
}

void OutgoingTunnelConnection::proceedWithConnection(
    UdtStreamSocket* connectionPtr,
    std::chrono::milliseconds timeout)
{
    NX_VERBOSE(this, "cross-nat %1. timeout %2", m_connectionId, timeout);

    // We are in connectionPtr socket aio thread.

    // If we are in this method, then connectionPtr is certainly alive.
    NX_MUTEX_LOCKER lk(&m_mutex);

    // Checking that connection has not been cancelled by pleaseStop.
    if (m_ongoingConnections.find(connectionPtr) == m_ongoingConnections.end())
        return; //< Connection has been cancelled by pleaseStop, ignoring...

    NX_VERBOSE(this, "cross-nat %1. Initiating async connect to %2 with timeout %3",
        m_connectionId, m_remoteHostAddress, timeout);

    const bool timoutPresent = timeout > std::chrono::milliseconds::zero();

    connectionPtr->connectAsync(
        m_remoteHostAddress,
        [this, connectionPtr, timoutPresent](
            SystemError::ErrorCode errorCode)
        {
            // Cancelling timer.
            if (timoutPresent)
                connectionPtr->cancelIOSync(aio::etTimedOut);

            onConnectCompleted(connectionPtr, errorCode);
        });

    if (timoutPresent)
    {
        connectionPtr->registerTimer(
            timeout,
            [connectionPtr, this]
            {
                connectionPtr->cancelIOSync(aio::etNone);   //< Cancelling connect.
                onConnectCompleted(connectionPtr, SystemError::timedOut);
            });
    }
}

void OutgoingTunnelConnection::onConnectCompleted(
    UdtStreamSocket* connectionPtr,
    SystemError::ErrorCode errorCode)
{
    // Called from connectionPtr completion handler ensuring connectionPtr can be safely freed
    // in any thread.

    post([this, connectionPtr, errorCode]() { reportConnectResult(connectionPtr, errorCode); });
}

void OutgoingTunnelConnection::reportConnectResult(
    UdtStreamSocket* connectionPtr,
    SystemError::ErrorCode errorCode)
{
    NX_VERBOSE(this, "cross-nat %1. result: %2", m_connectionId, SystemError::toString(errorCode));

    // We are in object's thread.

    NX_MUTEX_LOCKER lk(&m_mutex);

    auto connectionIter = m_ongoingConnections.find(connectionPtr);
    if (connectionIter == m_ongoingConnections.end())
        return; // This can happen after OutgoingTunnelConnection::pleaseStop has been called.

    ConnectionContext connectionContext = std::move(connectionIter->second);
    m_ongoingConnections.erase(connectionIter);
    lk.unlock();

    if (errorCode == SystemError::noError)
    {
        // Binding to desired aio thread.
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
    NX_DEBUG(this, "cross-nat %1. Control connection has been closed: %2",
        m_connectionId, SystemError::toString(closeReason));

    auto controlConnection = std::move(m_controlConnection);

    if (m_controlConnectionClosedHandler)
    {
        auto controlConnectionClosedHandler = std::move(m_controlConnectionClosedHandler);
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
            NX_DEBUG(this, "cross-nat %1. Received SYN response with unexpected connection id: %2 vs %1. "
                "Verification failed. Closing connection...", m_connectionId, synAck.connectSessionId);
            m_controlConnection->closeConnection(SystemError::connectionReset);
            return;
        }
    }
    else
    {
        NX_DEBUG(this, "cross-nat %1. Failed to parse SYN-ACK", m_connectionId);
        return; //< Ignoring.
    }

    m_lastAckTime = nx::utils::monotonicTime();

    if (!m_isVerified)
    {
        NX_VERBOSE(this, "cross-nat %1. Control connection has been verified", m_connectionId);
        m_isVerified = true;
        startKeepAlive();
    }

    // Received keep-alive here. The keep-alive timer is already running.
}

void OutgoingTunnelConnection::startKeepAlive()
{
    NX_VERBOSE(this, "cross-nat %1. Starting keep-alive with %2", m_connectionId,
        m_settings.keepAliveOptions);

    // Disabling recv timeout and solely relying on keep-alive from now on.
    m_controlConnection->socket()->setRecvTimeout(std::chrono::milliseconds::zero());

    // Starting keep-alive timer.
    if (m_settings.keepAliveOptions.probeSendPeriod > std::chrono::milliseconds::zero() &&
        m_settings.keepAliveOptions.probeCount > 0)
    {
        m_keepAliveTimer.start(
            m_settings.keepAliveOptions.probeSendPeriod,
            [this]() { performKeepAliveIteration(); });
    }
}

void OutgoingTunnelConnection::performKeepAliveIteration()
{
    if (!m_controlConnection)
        return;

    const auto timeSinceLastAck = std::chrono::floor<std::chrono::milliseconds>(
        nx::utils::monotonicTime() - m_lastAckTime);
    if (timeSinceLastAck > m_settings.keepAliveOptions.maxDelay())
    {
        NX_DEBUG(this, "cross-nat %1. Closing connection due to keep-alive timeout of %2",
            m_connectionId, timeSinceLastAck);
        m_controlConnection->closeConnection(SystemError::connectionReset);
        return;
    }

    sendKeepAliveProbe();
}

void OutgoingTunnelConnection::sendKeepAliveProbe()
{
    hpm::api::UdpHolePunchingSynRequest syn;
    stun::Message message;
    syn.serialize(&message);
    m_controlConnection->sendMessage(std::move(message));
}

} // namespace nx::network::cloud::udp
