#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "reverse_acceptor.h"
#include "../../mediator_client_connections.h"

namespace nx {
namespace network {
namespace cloud {

class OutgoingTunnelPool;

namespace tcp {

class ReverseConnectionSource;
class ReverseConnectionHolder;

/**
 * Registers on mediator for NXRC connections, accept and stores them.
 *
 * Is is safe to remove this object from any non-AIO thread (destructor blocks to wait connection
 * handlers even if it's stopped).
 */
class NX_NETWORK_API ReverseConnectionPool:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    ReverseConnectionPool(
        aio::AIOService* aioService,
        const OutgoingTunnelPool& outgoingTunnelPool,
        std::unique_ptr<hpm::api::AbstractMediatorClientTcpConnection> mediatorConnection);
    virtual ~ReverseConnectionPool() override;

    ReverseConnectionPool(const ReverseConnectionPool&) = delete;
    ReverseConnectionPool(ReverseConnectionPool&&) = delete;
    ReverseConnectionPool& operator=(const ReverseConnectionPool&) = delete;
    ReverseConnectionPool& operator=(ReverseConnectionPool&&) = delete;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    bool start(HostAddress publicIp, uint16_t port, bool waitForRegistration = false);
    nx::network::SocketAddress address() const;

    std::shared_ptr<ReverseConnectionSource> getConnectionSource(const String& hostName);

    // TODO: make is configurable for each client? can it be usefull?
    void setPoolSize(boost::optional<size_t> value);
    void setHttpConnectionInactivityTimeout(std::chrono::milliseconds inactivityTimeout);
    void setKeepAliveOptions(boost::optional<KeepAliveOptions> value);
    void setStartTimeout(std::chrono::milliseconds value);

protected:
    virtual void stopWhileInAioThread() override;

private:
    bool registerOnMediator(bool waitForRegistration = false);
    std::shared_ptr<ReverseConnectionHolder> getOrCreateHolder(const String& hostName);

    const OutgoingTunnelPool& m_outgoingTunnelPool;
    std::unique_ptr<hpm::api::AbstractMediatorClientTcpConnection> m_mediatorConnection;
    ReverseAcceptor m_acceptor;
    HostAddress m_publicIp;
    bool m_isReconnectHandlerSet;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::milliseconds m_startTimeout;

    mutable QnMutex m_mutex;
    typedef std::map<String /*name*/, std::shared_ptr<ReverseConnectionHolder>> HoldersByName;
    std::map<String /*suffix*/, HoldersByName> m_connectionHolders;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
