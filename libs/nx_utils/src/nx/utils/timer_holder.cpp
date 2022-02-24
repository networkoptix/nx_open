// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timer_holder.h"

namespace nx {
namespace utils {

TimerHolder::TimerHolder(nx::utils::TimerManager* timerManager):
    m_timerManager(timerManager)
{
}

TimerHolder::~TimerHolder()
{
    terminate();
}

void TimerHolder::addTimer(
    const TimerOwnerId& timerOwnerId,
    std::function<void()> callback,
    std::chrono::milliseconds delay)
{
    std::shared_ptr<TimerContext> timerContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_terminated)
            return;

        timerContext = timerContextUnsafe(timerOwnerId);
    }

    const auto setTimer =
        [&]()
        {
            if (m_terminated)
                return;

            if (timerContext->timerId)
                m_timerManager->deleteTimer(timerContext->timerId);

            timerContext->timerId = m_timerManager->addTimer(
                [timerContext, callback](TimerId timerId)
                {
                    NX_MUTEX_LOCKER lock(&timerContext->lock);
                    if (timerContext->timerId != timerId)
                        return;

                    callback();
                },
                delay);
        };

    if (m_timerManager == QThread::currentThread())
        return setTimer();

    NX_MUTEX_LOCKER lock(&timerContext->lock);
    setTimer();
}

void TimerHolder::cancelTimerSync(const TimerOwnerId& timerOwnerId)
{
    std::shared_ptr<TimerContext> timerContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        timerContext = timerContextUnsafe(timerOwnerId);
    }

    NX_MUTEX_LOCKER lock(&timerContext->lock);
    if (!timerContext->timerId)
        return;

    m_timerManager->deleteTimer(timerContext->timerId);
    timerContext->timerId = 0;
}

void TimerHolder::cancelAllTimersSync()
{
    decltype(m_timers) timers;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        timers = m_timers;
    }

    for (const auto& timerEntry: timers)
        cancelTimerSync(timerEntry.first);
}

void TimerHolder::terminate()
{
    m_terminated = true;
    cancelAllTimersSync();
}

std::shared_ptr<TimerHolder::TimerContext> TimerHolder::timerContextUnsafe(
    const TimerOwnerId& timerOwnerId)
{
    auto timerContext = m_timers[timerOwnerId];
    if (!timerContext)
    {
        timerContext = std::make_shared<TimerContext>();
        m_timers[timerOwnerId] = timerContext;
    }

    return timerContext;
}

} // namespace utils
} // namespace nx
