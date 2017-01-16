#pragma once

#include <nx/network/cloud/mediator_connections.h>

#include "reverse_acceptor.h"

namespace nx {
namespace network {
namespace cloud {
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
    public QnStoppableAsync
{
public:
    typedef hpm::api::MediatorClientTcpConnection MediatorConnection;
    explicit ReverseConnectionPool(std::unique_ptr<MediatorConnection> mediatorConnection);
    ~ReverseConnectionPool();

    ReverseConnectionPool(const ReverseConnectionPool&) = delete;
    ReverseConnectionPool(ReverseConnectionPool&&) = delete;
    ReverseConnectionPool& operator=(const ReverseConnectionPool&) = delete;
    ReverseConnectionPool& operator=(ReverseConnectionPool&&) = delete;

    bool start(HostAddress publicIp, uint16_t port, bool waitForRegistration = false);
    uint16_t port() const;

    std::shared_ptr<ReverseConnectionSource> getConnectionSource(const String& hostName);
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    // TODO: make is configurable for each client? can it be usefull?
    void setPoolSize(boost::optional<size_t> value);
    void setKeepAliveOptions(boost::optional<KeepAliveOptions> value);

private:
    bool registerOnMediator(bool waitForRegistration = false);
    std::shared_ptr<ReverseConnectionHolder> getOrCreateHolder(const String& hostName);

    const std::unique_ptr<MediatorConnection> m_mediatorConnection;
    ReverseAcceptor m_acceptor;
    HostAddress m_publicIp;
    bool m_isReconnectHandlerSet;

    mutable QnMutex m_mutex;
    typedef std::map<String /*name*/, std::shared_ptr<ReverseConnectionHolder>> HoldersByName;
    std::map<String /*suffix*/, HoldersByName> m_connectionHolders;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
