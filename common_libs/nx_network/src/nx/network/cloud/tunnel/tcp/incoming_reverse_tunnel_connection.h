#pragma once

#include <nx/network/cloud/tunnel/abstract_incoming_tunnel_connection.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connector.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/object_destruction_flag.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
 * Sustains desired NXRC/1.0 connections count and returns sockets when some usefull data is
 * avaliable on them.
 */
class NX_NETWORK_API IncomingReverseTunnelConnection:
    public AbstractIncomingTunnelConnection
{
    using base_type = AbstractIncomingTunnelConnection;

public:
    using StartHandler = std::function<void(SystemError::ErrorCode)>;

    IncomingReverseTunnelConnection(
        String selfHostName, String targetHostName, SocketAddress proxyServiceEndpoint);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Initiates connectors spawn.
     * @param handler is called when tunnel is ready to use or failed.
     */
    void start(RetryPolicy policy, StartHandler handler);
    void setHttpTimeouts(nx::network::http::AsyncClient::Timeouts timeouts);

    void accept(AcceptHandler handler) override;

private:
    const String m_selfHostName;
    const String m_targetHostName;
    const SocketAddress m_proxyServiceEndpoint;

    nx::network::http::AsyncClient::Timeouts m_httpTimeouts;
    size_t m_expectedPoolSize = 1;
    boost::optional<KeepAliveOptions> m_keepAliveOptions;

    std::unique_ptr<RetryTimer> m_timer;
    std::list<std::unique_ptr<ReverseConnector>> m_connectors;
    std::list<std::unique_ptr<BufferedStreamSocket>> m_sockets;

    StartHandler m_startHandler;
    AcceptHandler m_acceptHandler;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    virtual void stopWhileInAioThread() override;

    void spawnConnectorIfNeeded();
    void saveConnection(std::unique_ptr<ReverseConnector> connector);
    void monitorSocket(std::list<std::unique_ptr<BufferedStreamSocket>>::iterator socketIt);
    bool isExhausted() const;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
