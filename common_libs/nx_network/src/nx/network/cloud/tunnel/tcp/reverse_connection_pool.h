#pragma once

#include <nx/network/cloud/mediator_connections.h>

#include "reverse_acceptor.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

class ReverseConnectionHolder;

/**
 * Registers on mediator for NXRC connections, accept and stores them.
 */
class NX_NETWORK_API ReverseConnectionPool:
    public QnStoppableAsync
{
public:
    typedef hpm::api::MediatorClientTcpConnection MediatorConnection;
    explicit ReverseConnectionPool(std::shared_ptr<MediatorConnection> mediatorConnection);

    ReverseConnectionPool(const ReverseConnectionPool&) = delete;
    ReverseConnectionPool(ReverseConnectionPool&&) = delete;
    ReverseConnectionPool& operator=(const ReverseConnectionPool&) = delete;
    ReverseConnectionPool& operator=(ReverseConnectionPool&&) = delete;

    bool start(const SocketAddress& address, bool waitForRegistration = false);
    std::shared_ptr<ReverseConnectionHolder> getConnectionHolder(const String& hostName);
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    // TODO: make is configurable for each client? can it be usefull?
    void setPoolSize(boost::optional<size_t> value);
    void setKeepAliveOptions(boost::optional<KeepAliveOptions> value);

private:
    bool registerOnMediator(bool waitForRegistration = false);
    std::shared_ptr<ReverseConnectionHolder> getHolder(const String& hostName, bool mayCreate);

    const std::shared_ptr<MediatorConnection> m_mediatorConnection;
    std::unique_ptr<ReverseAcceptor> m_acceptor;

    mutable QnMutex m_mutex;
    std::map<String, std::shared_ptr<ReverseConnectionHolder>> m_connectionHolders;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
