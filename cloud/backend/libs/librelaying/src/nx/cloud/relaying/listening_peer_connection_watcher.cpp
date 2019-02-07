#include "listening_peer_connection_watcher.h"

#include <nx/utils/log/log.h>

namespace nx::cloud::relaying {

ListeningPeerConnectionWatcher::ListeningPeerConnectionWatcher(
    std::unique_ptr<network::AbstractStreamSocket> connection)
    :
    m_connection(std::move(connection))
{
    bindToAioThread(m_connection->getAioThread());
}

void ListeningPeerConnectionWatcher::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void ListeningPeerConnectionWatcher::start(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_connectionClosedHandler = std::move(handler);
            monitoringConnectionForClosure();
        });
}

void ListeningPeerConnectionWatcher::startTunnel(
    const ClientInfo& clientInfo,
    OpenTunnelHandler handler)
{
    relay::api::OpenTunnelNotification notification;
    notification.setClientEndpoint(clientInfo.endpoint);
    notification.setClientPeerName(clientInfo.peerName.c_str());

    dispatch(
        [this, notification = std::move(notification),
            handler = std::move(handler)]() mutable
        {
            auto notificationBuffer = std::make_shared<nx::Buffer>(
                notification.toHttpMessage().toString());

            if (!m_connection)
            {
                post([this, handler = std::move(handler)]()
                    { handler(SystemError::connectionReset, nullptr); });
                return;
            }

            m_connection->sendAsync(
                *notificationBuffer,
                [this, notificationBuffer, handler = std::move(handler)](
                    SystemError::ErrorCode sysErrorCode,
                    std::size_t /*bytesSent*/) mutable
                {
                    m_connection->cancelIOSync(network::aio::etNone);
                    handler(sysErrorCode, std::exchange(m_connection, nullptr));
                });
        });
}

void ListeningPeerConnectionWatcher::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_connection.reset();
}

void ListeningPeerConnectionWatcher::monitoringConnectionForClosure()
{
    constexpr int readBufferSize = 1; //< We need only detect connection closure.

    m_readBuffer.clear();
    m_readBuffer.reserve(readBufferSize);
    m_connection->readSomeAsync(
        &m_readBuffer,
        [this](auto&&... args) { onReadCompletion(std::forward<decltype(args)>(args)...); });
}

void ListeningPeerConnectionWatcher::onReadCompletion(
    SystemError::ErrorCode sysErrorCode,
    std::size_t bytesRead)
{
    const bool isConnectionClosed =
        sysErrorCode == SystemError::noError && bytesRead == 0;

    if (isConnectionClosed ||
        (sysErrorCode != SystemError::noError &&
            nx::network::socketCannotRecoverFromError(sysErrorCode)))
    {
        m_connection.reset();
        nx::utils::swapAndCall(
            m_connectionClosedHandler,
            isConnectionClosed ? SystemError::connectionReset : sysErrorCode);
        return;
    }

    // TODO: #ak What if some data has been received?

    monitoringConnectionForClosure();
}

} // namespace nx::cloud::relaying
