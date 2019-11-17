#include "event_loop_timer.h"

#include <QtCore/QThread>

#include <limits>

namespace nx::utils {

using namespace std::chrono;
using namespace std::chrono_literals;

EventLoopTimer::EventLoopTimer(milliseconds checkPeriod):
    m_timer(this),
    m_checkPeriod(checkPeriod)
{
    NX_ASSERT(checkPeriod >= 1ms && checkPeriod.count() < std::numeric_limits<int>::max());
    m_timer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, this, &EventLoopTimer::onTimer);
}

void EventLoopTimer::start(milliseconds timeout, MoveOnlyFunc<void()> handler)
{
    NX_ASSERT(timeout >= std::chrono::milliseconds::zero());
    NX_CRITICAL(handler);

    std::lock_guard<std::mutex> lock(m_mutex);

    m_handler = std::move(handler);
    m_leftTimeout = timeout;
    m_startPoint = steady_clock::now();
    m_timer.start(std::min(m_leftTimeout, m_checkPeriod));
}

void EventLoopTimer::onTimer()
{
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - m_startPoint);

    std::unique_lock<std::mutex> lock(m_mutex);
    m_leftTimeout -= elapsed;
    if (m_leftTimeout <= 0ms)
    {
        lock.unlock();
        m_handler();
        return;
    }

    m_startPoint = steady_clock::now();
    m_timer.start(std::min(m_leftTimeout, m_checkPeriod));
}

} // namespace nx::utils
