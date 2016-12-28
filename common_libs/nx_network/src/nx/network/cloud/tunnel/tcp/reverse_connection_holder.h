#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

class NX_NETWORK_API ReverseConnectionSource
{
public:
    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> Handler;

    virtual size_t socketCount() const = 0;
    virtual void takeSocket(std::chrono::milliseconds timeout, Handler handler) = 0;
};

/**
 * Keeps all user NXRC connections and moniors if they close.
 */
class NX_NETWORK_API ReverseConnectionHolder:
    public ReverseConnectionSource,
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

    /** @note Can only be called from bould AIO thread. */
    void saveSocket(std::unique_ptr<AbstractStreamSocket> socket);

    virtual size_t socketCount() const override;
    virtual void takeSocket(std::chrono::milliseconds timeout, Handler handler) override;

private:
    void startCleanupTimer(std::chrono::milliseconds timeLeft);
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
