/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#include "outgoing_tunnel_udt_connection.h"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <utils/common/cpp14.h>


namespace nx {
namespace network {
namespace cloud {

OutgoingTunnelUdtConnection::OutgoingTunnelUdtConnection(
    nx::String connectionId,
    std::unique_ptr<UdtStreamSocket> udtConnection,
    UdpHolePunchingTimeouts timeouts)
:
    m_connectionId(std::move(connectionId)),
    m_remoteHostAddress(udtConnection->getForeignAddress()),
    m_controlConnection(
        std::make_unique<ConnectionType>(this, std::move(udtConnection))),
    m_timeouts(timeouts),
    m_pleaseStopHasBeenCalled(false),
    m_pleaseStopCompleted(false)
{
    m_controlConnection->setMessageHandler(
        std::bind(&OutgoingTunnelUdtConnection::onStunMessageReceived,
            this, std::placeholders::_1));
    m_controlConnection->socket()->registerTimer(
        m_timeouts.maxConnectionInactivityPeriod(),
        std::bind(&OutgoingTunnelUdtConnection::onKeepAliveTimeout, this));
}

OutgoingTunnelUdtConnection::OutgoingTunnelUdtConnection(
    nx::String connectionId,
    std::unique_ptr<UdtStreamSocket> udtConnection)
:
    OutgoingTunnelUdtConnection(
        std::move(connectionId),
        std::move(udtConnection),
        UdpHolePunchingTimeouts())
{
}

OutgoingTunnelUdtConnection::~OutgoingTunnelUdtConnection()
{
    NX_ASSERT(m_pleaseStopHasBeenCalled && m_pleaseStopCompleted);
}

void OutgoingTunnelUdtConnection::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> userCompletionHandler)
{
    //caller MUST guarantee that no calls to establishNewConnection can follow 
        //and establishNewConnection has returned
    m_pleaseStopHasBeenCalled = true;
    auto completionHandler = 
        [this, userCompletionHandler = std::move(userCompletionHandler)]()
        {
            m_pleaseStopCompleted = true;
            userCompletionHandler();
        };

    m_aioTimer.post(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            //cancelling ongoing connects
            QnMutexLocker lk(&m_mutex);
            std::map<UdtStreamSocket*, ConnectionContext> ongoingConnections;
            ongoingConnections.swap(m_ongoingConnections);
            lk.unlock();

            BarrierHandler completionHandlerCaller(std::move(completionHandler));
            for (auto& connectionContext: ongoingConnections)
            {
                connectionContext.second.connection->pleaseStop(
                    [connectionContext = std::move(connectionContext),
                        handler = completionHandlerCaller.fork()]()
                    {
                        connectionContext.second.completionHandler(
                            SystemError::interrupted,
                            nullptr,
                            true);
                        handler();
                    });
            }
            auto controlConnection = std::move(m_controlConnection);
            if (controlConnection)
            {
                auto controlConnectionPtr = controlConnection.get();
                controlConnectionPtr->pleaseStop(
                    [handler = completionHandlerCaller.fork(), 
                        controlConnection = std::move(controlConnection)]() mutable
                    {
                        controlConnection.reset();
                        handler();
                    });
            }
            m_aioTimer.pleaseStopSync();
        });
}

void OutgoingTunnelUdtConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    NX_ASSERT(!m_pleaseStopHasBeenCalled);

    NX_LOGX(lm("connection %1. New stream socket has been requested")
        .arg(m_connectionId), cl_logDEBUG2);

    auto newConnection = std::make_unique<UdtStreamSocket>();
    if (!socketAttributes.applyTo(newConnection.get()) ||
        !newConnection->setNonBlockingMode(true))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("connection %1. Failed to apply socket options to new connection. %2")
            .arg(m_connectionId).arg(SystemError::toString(errorCode)), cl_logDEBUG1);
        m_aioTimer.post(
            [this, handler = move(handler),
                errorCode = SystemError::getLastOSErrorCode()]() mutable
            {
                handler(errorCode, nullptr, m_controlConnection != nullptr);
            });
        return;
    }

    QnMutexLocker lk(&m_mutex);

    auto connectionPtr = newConnection.get();
    m_ongoingConnections.emplace(
        connectionPtr,
        ConnectionContext{std::move(newConnection), std::move(handler)});
    
    //has to start connect and timer atomically, 
        //but this can be done from socket's aio thread only
        //So, switching to newConnection's aio thread
    connectionPtr->post(
        std::bind(
            &OutgoingTunnelUdtConnection::proceedWithConnection, this,
            connectionPtr, timeout));

    NX_ASSERT(!m_pleaseStopHasBeenCalled);
}

void OutgoingTunnelUdtConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_controlConnectionClosedHandler = std::move(handler);
}

void OutgoingTunnelUdtConnection::proceedWithConnection(
    UdtStreamSocket* connectionPtr,
    std::chrono::milliseconds timeout)
{
    //we are in connectionPtr socket aio thread

    //if we are in this method, then connectionPtr is certainly alive
    QnMutexLocker lk(&m_mutex);
    //checking that connection has not been cancelled by pleaseStop
    if (m_ongoingConnections.find(connectionPtr) == m_ongoingConnections.end())
        return; //connection has been cancelled by pleaseStop, ignoring...

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

void OutgoingTunnelUdtConnection::onConnectCompleted(
    UdtStreamSocket* connectionPtr,
    SystemError::ErrorCode errorCode)
{
    //called from \a connectionPtr completion handler

    //ensuring connectionPtr can be safely freed in any thread
    connectionPtr->post(
        [this, connectionPtr, errorCode]()
        {
            //switching execution to m_aioTimer's thread
            m_aioTimer.post(
                std::bind(
                    &OutgoingTunnelUdtConnection::reportConnectResult, this,
                    connectionPtr, errorCode));
        });
}

void OutgoingTunnelUdtConnection::reportConnectResult(
    UdtStreamSocket* connectionPtr,
    SystemError::ErrorCode errorCode)
{
    //we are in m_aioTimer's thread

    QnMutexLocker lk(&m_mutex);
    auto connectionIter = m_ongoingConnections.find(connectionPtr);
    if (connectionIter == m_ongoingConnections.end())
        return; //this can happen after OutgoingTunnelUdtConnection::pleaseStop has been called

    ConnectionContext connectionContext = std::move(connectionIter->second);
    m_ongoingConnections.erase(connectionIter);
    lk.unlock();

    connectionContext.completionHandler(
        errorCode,
        errorCode == SystemError::noError
            ? std::move(connectionContext.connection)
            : std::unique_ptr<AbstractStreamSocket>(),
        m_controlConnection != nullptr);
}

void OutgoingTunnelUdtConnection::closeConnection(
    SystemError::ErrorCode closeReason,
    ConnectionType* connection)
{
    NX_LOGX(lm("session %1. Control connection has been closed: %2")
        .arg(m_connectionId).arg(SystemError::toString(closeReason)),
        cl_logDEBUG1);

    if (m_controlConnectionClosedHandler)
    {
        auto controlConnectionClosedHandler =
            std::move(m_controlConnectionClosedHandler);
        controlConnectionClosedHandler();
    }

    auto controlConnection = std::move(m_controlConnection);
    if (!controlConnection)
        return; //pleaseStop has already been called...

    //we are in connection's aio thread
    NX_ASSERT(connection == controlConnection.get());
}

void OutgoingTunnelUdtConnection::onStunMessageReceived(
    nx::stun::Message message)
{
    //TODO #ak
}

void OutgoingTunnelUdtConnection::onKeepAliveTimeout()
{
    closeConnection(SystemError::notConnected, m_controlConnection.get());
}

} // namespace cloud
} // namespace network
} // namespace nx
