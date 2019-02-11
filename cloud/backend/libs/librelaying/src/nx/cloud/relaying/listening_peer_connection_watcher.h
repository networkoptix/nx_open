#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>

namespace nx::cloud::relaying {

struct ClientInfo
{
    std::string relaySessionId;
    network::SocketAddress endpoint;
    std::string peerName;
};

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
        const std::string& peerProtocolVersion);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> connectionClosureHandler);

    void startTunnel(const ClientInfo& clientInfo, OpenTunnelHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<network::AbstractStreamSocket> m_connection;
    const std::string m_peerProtocolVersion;
    nx::Buffer m_readBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_connectionClosedHandler;

    void monitoringConnectionForClosure();

    void onReadCompletion(
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesRead);
};

} // namespace nx::cloud::relaying
