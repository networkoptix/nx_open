#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>

namespace nx::cloud::relaying {

struct ClientInfo
{
    std::string relaySessionId;
    network::SocketAddress endpoint;
    std::string peerName;
};

/**
 * NOTE: The first keep-alive notification is sent right after call to
 * ListeningPeerConnectionWatcher::start().
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

    void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> connectionClosureHandler);

    void startTunnel(const ClientInfo& clientInfo, OpenTunnelHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<network::AbstractStreamSocket> m_connection;
    const std::string m_peerProtocolVersion;
    const std::chrono::milliseconds m_keepAliveProbePeriod;
    nx::Buffer m_readBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectionClosedHandler;
    network::aio::Timer m_timer;

    void monitoringConnectionForClosure();

    bool peerSupportsKeepAliveProbe() const;

    void startSendingKeepAliveProbes();

    void sendKeepAliveProbe();

    template<typename Notification, typename Handler>
    // requires std::is_void<std::invoke_result_t<handler, SystemError::ErrorCode>::type>::value
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
