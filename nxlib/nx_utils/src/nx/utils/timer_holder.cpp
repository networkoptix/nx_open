#include "timer_holder.h"

namespace nx {
namespace utils {

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
        QnMutexLocker lock(&m_mutex);
        if (m_terminated)
            return;

        timerContext = timerContextUnsafe(timerOwnerId);
    }

    QnMutexLocker lock(&timerContext->lock);
    if (m_terminated)
        return;

    if (timerContext->timerId)
        TimerManager::instance()->deleteTimer(timerContext->timerId);

    timerContext->timerId = TimerManager::instance()->addTimer(
        [timerContext, callback](TimerId timerId)
        {
            QnMutexLocker lock(&timerContext->lock);
            if (timerContext->timerId != timerId)
                return;

            callback();
        },
        delay);
}

void TimerHolder::cancelTimerSync(const TimerOwnerId& timerOwnerId)
{
    std::shared_ptr<TimerContext> timerContext;
    {
        QnMutexLocker lock(&m_mutex);
        timerContext = timerContextUnsafe(timerOwnerId);
    }

    QnMutexLocker lock(&timerContext->lock);
    if (!timerContext->timerId)
        return;

    TimerManager::instance()->deleteTimer(timerContext->timerId);
    timerContext->timerId = 0;
}

void TimerHolder::cancelAllTimersSync()
{
    decltype(m_timers) timers;
    {
        QnMutexLocker lock(&m_mutex);
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
