// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    TimerEventHandler timerFunc)
{
    NX_CRITICAL(timerFunc);

    // m_aioService.registerTimer currently does not support zero timeouts,
    // so replacing zero delay with a minimum supported value.
    // TODO: #akolesnikov Use post in this case.
    if (timeout == std::chrono::milliseconds::zero())
        timeout = std::chrono::milliseconds(1);

    m_timeout = timeout;
    m_timerStartClock = std::chrono::steady_clock::now();

    dispatch(
        [this, timeout, handler = std::move(timerFunc)]() mutable
        {
            m_handler = std::move(handler);
            ++m_internalTimerId;
            m_aioService.registerTimer(&pollable(), timeout, this);
        });
}

cf::future<cf::unit> Timer::start(std::chrono::milliseconds timeout)
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();

    start(timeout,
        [promise = std::move(promise)]() mutable { promise.set_value(cf::unit()); });

    return future
        .then(cf::translate_broken_promise_to_operation_canceled);
}

std::optional<std::chrono::nanoseconds> Timer::timeToEvent() const
{
    if (!m_timerStartClock)
        return std::nullopt;

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

cf::future<cf::unit> Timer::cancel()
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();

    cancelAsync([promise = std::move(promise)]() mutable { promise.set_value(cf::unit()); });

    return future
        .then(cf::translate_broken_promise_to_operation_canceled);
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
    m_aioService.stopMonitoring(&pollable(), EventType::etTimedOut);
    m_timerStartClock.reset();
    m_handler = nullptr;
}

void Timer::eventTriggered(Pollable* sock, aio::EventType eventType) throw()
{
    NX_ASSERT(sock == &pollable() && eventType == aio::EventType::etTimedOut);
    NX_CRITICAL(m_handler);

    decltype(m_handler) handler;
    m_handler.swap(handler);

    nx::utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);
    const int internalTimerId = m_internalTimerId;

    m_timerStartClock = std::nullopt;
    handler();

    if (watcher.interrupted())
        return;

    if (internalTimerId == m_internalTimerId)
        m_aioService.stopMonitoring(&pollable(), EventType::etTimedOut);
}

} // namespace aio
} // namespace network
} // namespace nx
