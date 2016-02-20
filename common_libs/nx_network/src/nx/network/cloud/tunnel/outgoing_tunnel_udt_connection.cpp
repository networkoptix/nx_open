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
    std::unique_ptr<UdtStreamSocket> udtConnection)
:
    m_connectionId(std::move(connectionId)),
    m_controlConnection(std::move(udtConnection)),
    m_remoteHostAddress(m_controlConnection->getForeignAddress()),
    m_terminated(false)
{
    //TODO #ak accepting STUN messages on m_controlConnection
}

void OutgoingTunnelUdtConnection::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    //caller MUST guarantee that no calls to establishNewConnection can follow 
        //and establishNewConnection has returned
    m_terminated = true;

    m_aioTimer.post(
        [this, completionHandler = move(completionHandler)]() mutable
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
            m_controlConnection->pleaseStop(completionHandlerCaller.fork());
            m_aioTimer.pleaseStopSync();
        });
}

void OutgoingTunnelUdtConnection::establishNewConnection(
    boost::optional<std::chrono::milliseconds> timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    assert(!m_terminated);

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
            [handler = move(handler),
                errorCode = SystemError::getLastOSErrorCode()]() mutable
            {
                handler(errorCode, nullptr, true);
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
    connectionPtr->post(    //switching to connectionPtr's aio thread
        [this, connectionPtr, timeout]
        {
            QnMutexLocker lk(&m_mutex);
            //checking that connection has not been cancelled by pleaseStop
            if (m_ongoingConnections.find(connectionPtr) == m_ongoingConnections.end())
                return; //connection has been cancelled by pleaseStop, ignoring...

            connectionPtr->connectAsync(
                m_remoteHostAddress,
                [this, connectionPtr, timoutPresent = static_cast<bool>(timeout)](
                    SystemError::ErrorCode errorCode)
                {
                    //cancelling timer
                    if (timoutPresent)
                        connectionPtr->cancelIOSync(aio::etTimedOut);

                    onConnectCompleted(connectionPtr, errorCode);
                });

            if (timeout)
                connectionPtr->registerTimer(
                    *timeout,
                    [connectionPtr, this]
                    {
                        connectionPtr->cancelIOSync(aio::etNone);   //cancelling connect
                        onConnectCompleted(connectionPtr, SystemError::timedOut);
                    });
        });

    assert(!m_terminated);
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
        true);
}

} // namespace cloud
} // namespace network
} // namespace nx
