#include "listening_peer_connection_watcher.h"

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>

namespace nx::cloud::relaying {

ListeningPeerConnectionWatcher::ListeningPeerConnectionWatcher(
    std::unique_ptr<network::AbstractStreamSocket> connection,
    const std::string& peerProtocolVersion,
    std::chrono::milliseconds keepAliveProbePeriod)
    :
    m_connection(std::move(connection)),
    m_peerProtocolVersion(peerProtocolVersion),
    m_keepAliveProbePeriod(keepAliveProbePeriod)
{
    bindToAioThread(m_connection->getAioThread());
}

void ListeningPeerConnectionWatcher::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_connection)
        m_connection->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void ListeningPeerConnectionWatcher::start(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_connectionClosedHandler = std::move(handler);
            monitoringConnectionForClosure();

            if (peerSupportsKeepAliveProbe())
                sendKeepAliveProbe();
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
            cancelKeepAlive();

            if (!peerSupportsKeepAliveProbe() || m_firstKeepAliveSent)
                return startTunnel(std::move(notification), std::move(handler));

            sendKeepAliveProbe(
                [this, notification = std::move(notification), handler = std::move(handler)](
                    SystemError::ErrorCode resultCode) mutable
                {
                    if (resultCode != SystemError::noError)
                        return nx::utils::swapAndCall(handler, resultCode, nullptr);

                    startTunnel(std::move(notification), std::move(handler));
                });
        });
}

void ListeningPeerConnectionWatcher::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_connection.reset();
    m_timer.pleaseStopSync();
}

void ListeningPeerConnectionWatcher::monitoringConnectionForClosure()
{
    constexpr int readBufferSize = 128; //< We need only detect connection closure.

    m_readBuffer.clear();
    m_readBuffer.reserve(readBufferSize);
    m_connection->readSomeAsync(
        &m_readBuffer,
        [this](auto&&... args) { onReadCompletion(std::forward<decltype(args)>(args)...); });
}

bool ListeningPeerConnectionWatcher::peerSupportsKeepAliveProbe() const
{
    if (m_peerProtocolVersion.empty())
        return false;

    return nx::utils::SoftwareVersion(m_peerProtocolVersion.c_str()) >=
        nx::utils::SoftwareVersion(0, 1);
}

void ListeningPeerConnectionWatcher::sendKeepAliveProbe()
{
    sendKeepAliveProbe(
        [this](SystemError::ErrorCode /*resultCode*/)
        {
            // TODO: #ak Report connection closure? Or rely on monitoringConnectionForClosure?
            scheduleKeepAliveProbeTimer();
        });
}

void ListeningPeerConnectionWatcher::sendKeepAliveProbe(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    relay::api::KeepAliveNotification notification;
    sendNotification(
        std::move(notification),
        [this, handler = std::move(handler)](SystemError::ErrorCode resultCode)
        {
            if (resultCode == SystemError::noError)
                m_firstKeepAliveSent = true;

            if (handler)
                handler(resultCode);
        });
}

void ListeningPeerConnectionWatcher::startTunnel(
    relay::api::OpenTunnelNotification notification,
    OpenTunnelHandler handler)
{
    sendNotification(
        std::move(notification),
        [this, handler = std::move(handler)](SystemError::ErrorCode resultCode)
        {
            if (m_connection)
                m_connection->cancelIOSync(network::aio::etNone);
            handler(resultCode, std::exchange(m_connection, nullptr));
        });
}

template<typename Notification, typename Handler>
// requires std::is_void<std::invoke_result_t<handler, SystemError::ErrorCode>::type>::value
void ListeningPeerConnectionWatcher::sendNotification(
    Notification notification,
    Handler handler)
{
    dispatch(
        [this, notification = std::move(notification),
            handler = std::move(handler)]() mutable
        {
            auto notificationBuffer = std::make_shared<nx::Buffer>(
                notification.toHttpMessage().toString());

            if (!m_connection)
            {
                post([this, handler = std::move(handler)]()
                    { handler(SystemError::connectionReset); });
                return;
            }

            m_connection->sendAsync(
                *notificationBuffer,
                [this, notificationBuffer, handler = std::move(handler)](
                    SystemError::ErrorCode sysErrorCode,
                    std::size_t /*bytesSent*/) mutable
                {
                    handler(sysErrorCode);
                });
        });
}

void ListeningPeerConnectionWatcher::scheduleKeepAliveProbeTimer()
{
    m_timer.start(
        m_keepAliveProbePeriod,
        [this]() { sendKeepAliveProbe(); });
}

void ListeningPeerConnectionWatcher::cancelKeepAlive()
{
    if (m_connection)
        m_connection->cancelIOSync(network::aio::etWrite);
    m_timer.pleaseStopSync();
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
        nx::utils::swapAndCall(
            m_connectionClosedHandler,
            isConnectionClosed ? SystemError::connectionReset : sysErrorCode);
        return;
    }

    // TODO: #ak What if some data has been received?

    monitoringConnectionForClosure();
}

} // namespace nx::cloud::relaying
