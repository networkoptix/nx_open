#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
 * Keeps all user NXRC connections and moniors if they close.
 */
class NX_NETWORK_API ReverseConnectionHolder: 
    public aio::BasicPollable
{
public:
    explicit ReverseConnectionHolder(aio::AbstractAioThread* aioThread);
    virtual ~ReverseConnectionHolder() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    ReverseConnectionHolder(const ReverseConnectionHolder&) = delete;
    ReverseConnectionHolder(ReverseConnectionHolder&&) = delete;
    ReverseConnectionHolder& operator=(const ReverseConnectionHolder&) = delete;
    ReverseConnectionHolder& operator=(ReverseConnectionHolder&&) = delete;

    void saveSocket(std::unique_ptr<AbstractStreamSocket> socket);
    size_t socketCount() const;

    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> Handler;

    void takeSocket(std::chrono::milliseconds timeout, Handler handler);

private:
    void monitorSocket(std::list<std::unique_ptr<AbstractStreamSocket>>::iterator it);

    std::atomic<size_t> m_socketCount;
    std::list<std::unique_ptr<AbstractStreamSocket>> m_sockets;
    std::multimap<std::chrono::steady_clock::time_point, Handler> m_handlers;
    std::unique_ptr<aio::Timer> m_timer;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
