#include "timer.h"

#include "aio_service.h"
#include "../socket_global.h"

namespace nx {
namespace network {
namespace aio {

Timer::Timer(aio::AbstractAioThread* aioThread):
    BasicPollable(aioThread),
    m_timeout(0),
    m_aioService(SocketGlobals::aioService()),
    m_internalTimerId(0)
{
}

Timer::~Timer()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
    NX_ASSERT(!m_aioService.isSocketBeingMonitored(&pollable()));
}

void Timer::start(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> timerFunc)
{
    // TODO: #ak m_aioService.registerTimer currently does not support zero timeouts, 
    // so using following hack
    if (timeout == std::chrono::milliseconds::zero())
        timeout = std::chrono::milliseconds(1); 

    m_handler = std::move(timerFunc);
    m_timeout = timeout;
    m_timerStartClock = std::chrono::steady_clock::now();

    QnMutexLocker lock(&m_mutex);
    ++m_internalTimerId;
    m_aioService.registerTimer(&pollable(), timeout, this);
}

boost::optional<std::chrono::nanoseconds> Timer::timeToEvent() const
{
    if (!m_timerStartClock)
        return boost::none;

    const auto elapsed = std::chrono::steady_clock::now() - *m_timerStartClock;
    return elapsed >= m_timeout
        ? std::chrono::nanoseconds::zero()
        : m_timeout - elapsed;
}

void Timer::cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void Timer::cancelSync()
{
    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
    }
    else
    {
        nx::utils::promise<void> cancelledPromise;
        cancelAsync([&cancelledPromise]() { cancelledPromise.set_value(); });
        cancelledPromise.get_future().wait();
    }
}

void Timer::stopWhileInAioThread()
{
    m_aioService.stopMonitoring(&pollable(), EventType::etTimedOut, true);
    m_handler = nullptr;
}

void Timer::eventTriggered(Pollable* sock, aio::EventType eventType) throw()
{
    NX_ASSERT(sock == &pollable() && eventType == aio::EventType::etTimedOut);
    NX_CRITICAL(m_handler);

    decltype(m_handler) handler;
    m_handler.swap(handler);

    nx::utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
    const int internalTimerId = m_internalTimerId;

    m_timerStartClock = boost::none;
    handler();

    if (watcher.objectDestroyed())
        return;

    QnMutexLocker lock(&m_mutex);
    if (internalTimerId == m_internalTimerId)
        m_aioService.stopMonitoring(&pollable(), EventType::etTimedOut, true);
}

} // namespace aio
} // namespace network
} // namespace nx
