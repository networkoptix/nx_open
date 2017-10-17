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
 * Keeps all user NXRC connections and monitors if they close.
 */
class NX_NETWORK_API ReverseConnectionHolder:
    public ReverseConnectionSource,
    public aio::BasicPollable
{
public:
    ReverseConnectionHolder(
        aio::AbstractAioThread* aioThread,
        const String& hostName);
    virtual ~ReverseConnectionHolder() override;

    ReverseConnectionHolder(const ReverseConnectionHolder&) = delete;
    ReverseConnectionHolder(ReverseConnectionHolder&&) = delete;
    ReverseConnectionHolder& operator=(const ReverseConnectionHolder&) = delete;
    ReverseConnectionHolder& operator=(ReverseConnectionHolder&&) = delete;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual size_t socketCount() const override;
        virtual void takeSocket(std::chrono::milliseconds timeout, Handler handler) override;

    /**
     * NOTE: Can only be called from object's AIO thread.
     */
    void saveSocket(std::unique_ptr<AbstractStreamSocket> socket);
    bool isActive() const;

private:
    void startCleanupTimer(std::chrono::milliseconds timeLeft);
    void monitorSocket(std::list<std::unique_ptr<AbstractStreamSocket>>::iterator it);
    const String m_hostName;
    std::atomic<size_t> m_socketCount;
    std::list<std::unique_ptr<AbstractStreamSocket>> m_sockets;
    std::multimap<std::chrono::steady_clock::time_point, Handler> m_handlers;
    std::unique_ptr<aio::Timer> m_timer;
    std::chrono::steady_clock::time_point m_prevConnectionTime;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
