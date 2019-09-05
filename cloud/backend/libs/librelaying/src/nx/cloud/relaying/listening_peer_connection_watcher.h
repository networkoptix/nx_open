#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_notifications.h>

namespace nx::cloud::relaying {

struct ClientInfo
{
    std::string relaySessionId;
    network::SocketAddress endpoint;
    std::string peerName;
};

/**
 * Monitors server connection until it is closed or passed to a client.
 * NOTE: The first keep-alive notification is sent right after call to
 * ListeningPeerConnectionWatcher::startMonitoringConnection().
 * Other keep-alive probes are sent after keepAliveProbePeriod.
 */
class NX_RELAYING_API ListeningPeerConnectionWatcher:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using OpenTunnelHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<network::AbstractStreamSocket>)>;

    ListeningPeerConnectionWatcher(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        const std::string& peerProtocolVersion,
        std::chrono::milliseconds keepAliveProbePeriod);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void startMonitoringConnection(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> connectionClosureHandler);

    /**
     * Prepares the connection to be used by a client (notifies the server)
     * and reports the connection via handler.
     */
    void openTunnel(const ClientInfo& clientInfo, OpenTunnelHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<network::AbstractStreamSocket> m_connection;
    const std::string m_peerProtocolVersion;
    const std::chrono::milliseconds m_keepAliveProbePeriod;
    nx::Buffer m_readBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectionClosedHandler;
    network::aio::Timer m_keepAliveProbeTimer;
    bool m_firstKeepAliveSent = false;

    void monitoringConnectionForClosure();

    bool peerSupportsKeepAliveProbe() const;

    void sendKeepAliveProbe();

    void sendKeepAliveProbe(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    void openTunnel(
        relay::api::OpenTunnelNotification notification,
        OpenTunnelHandler handler);

    template<typename Notification, typename Handler>
    // requires std::is_void_v<std::invoke_result_t<handler, SystemError::ErrorCode>>
    void sendNotification(
        Notification notification,
        Handler handler);

    void scheduleKeepAliveProbeTimer();

    void cancelKeepAlive();

    void onReadCompletion(
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesRead);
};

} // namespace nx::cloud::relaying
