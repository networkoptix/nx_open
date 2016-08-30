#pragma once

#include <nx/network/retry_timer.h>
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
    ReverseConnectionPool(std::shared_ptr<MediatorConnection> mediatorConnection);

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

/**
 * Keeps all user NXRC connections and moniors if the close.
 *
 * All methods are protected by the mutex including handler calls, so user shell not call any
 * methods from handler.
 */
class ReverseConnectionHolder
{
public:
    ReverseConnectionHolder(aio::AbstractAioThread* aioThread);
    void stopInAioThread();

    void newSocket(std::unique_ptr<AbstractStreamSocket> socket);
    size_t socketCount() const;

    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> Handler;

    void takeSocket(std::chrono::milliseconds timeout, Handler handler);

private:
    void monitorSocket(std::list<std::unique_ptr<AbstractStreamSocket>>::iterator it);

    aio::Timer m_timer;
    std::atomic<size_t> m_socketCount;
    std::list<std::unique_ptr<AbstractStreamSocket>> m_sockets;
    std::multimap<std::chrono::steady_clock::time_point, Handler> m_handlers;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
