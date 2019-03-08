#include "repetitive_timer.h"

namespace nx::network::aio {

RepetitiveTimer::RepetitiveTimer()
{
    bindToAioThread(getAioThread());
}

void RepetitiveTimer::bindToAioThread(AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
}

void RepetitiveTimer::pleaseStopSync()
{
    base_type::pleaseStopSync();
    m_cancelled.store(true, std::memory_order_relaxed);
}

void RepetitiveTimer::start(
    std::chrono::milliseconds timeout,
    TimerEventHandler timerFunc)
{
    m_timeout = timeout;
    m_timerFunc = std::exchange(timerFunc, nullptr);

    m_timer.start(m_timeout, [this]() { onTimerEvent(); });
}

void RepetitiveTimer::cancelSync()
{
    m_timer.cancelSync();
    m_cancelled.store(true, std::memory_order_relaxed);
}

void RepetitiveTimer::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_timer.pleaseStopSync();
}

void RepetitiveTimer::onTimerEvent()
{
    nx::utils::InterruptionFlag::Watcher watcher(&m_destructionFlag);

    m_timerFunc();

    if (watcher.interrupted() ||
        m_cancelled.exchange(false, std::memory_order_relaxed))
    {
        return;
    }

    m_timer.start(m_timeout, [this]() { onTimerEvent(); });
}

} // namespace nx::network::aio
