#pragma once

#include <nx/network/cloud/tunnel/abstract_incoming_tunnel_connection.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connector.h>
#include <nx/network/retry_timer.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
 * Sustains desired NXRC/1.0 connections count and returns sockets when some usefull data is
 * avaliable on them.
 */
class NX_NETWORK_API IncomingReverseTunnelConnection
:
    public AbstractIncomingTunnelConnection
{
public:
    IncomingReverseTunnelConnection(
        String selfHostName, String targetHostName, SocketAddress targetEndpoint);

    typedef std::function<void(SystemError::ErrorCode)> StartHandler;

    /** Initiates connectors spawn, @param handler is called when tunnel is ready to use or failed */
    void start(aio::AbstractAioThread* aioThread, RetryPolicy policy, StartHandler handler);
    void setHttpTimeouts(nx_http::AsyncHttpClient::Timeouts timeouts);

    void accept(AcceptHandler handler) override;
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    void spawnConnectorIfNeeded();
    void saveConnection(std::unique_ptr<ReverseConnector> connector);
    void monitorSocket(std::list<std::unique_ptr<BufferedStreamSocket>>::iterator socketIt);

    const String m_selfHostName;
    const String m_targetHostName;
    const SocketAddress m_targetEndpoint;

    nx_http::AsyncHttpClient::Timeouts m_httpTimeouts;
    size_t m_expectedPoolSize = 1;
    boost::optional<KeepAliveOptions> m_keepAliveOptions;

    std::unique_ptr<RetryTimer> m_timer;
    std::list<std::unique_ptr<ReverseConnector>> m_connectors;
    std::list<std::unique_ptr<BufferedStreamSocket>> m_sockets;

    StartHandler m_startHandler;
    AcceptHandler m_acceptHandler;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
