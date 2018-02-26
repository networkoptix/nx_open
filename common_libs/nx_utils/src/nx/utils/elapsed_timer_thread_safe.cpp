#include "elapsed_timer_thread_safe.h"

#include <mutex>

namespace nx {
namespace utils {

void ElapsedTimerThreadSafe::start()
{
    QnWriteLocker lock(&m_mutex);
    m_timer.start();
}

void ElapsedTimerThreadSafe::stop()
{
    QnWriteLocker lock(&m_mutex);
    m_timer.invalidate();
}

std::chrono::milliseconds ElapsedTimerThreadSafe::elapsedSinceStart() const
{
    QnReadLocker lock(&m_mutex);
    return std::chrono::milliseconds(m_timer.isValid() ? m_timer.elapsed() : 0);
}

bool ElapsedTimerThreadSafe::hasExpiredSinceStart(std::chrono::milliseconds ms) const
{
    QnReadLocker lock(&m_mutex);
    return m_timer.isValid() && m_timer.hasExpired(ms.count());
}

bool ElapsedTimerThreadSafe::isStarted() const
{
    QnReadLocker lock(&m_mutex);
    return m_timer.isValid();
}

std::chrono::milliseconds ElapsedTimerThreadSafe::elapsed() const
{
    QnReadLocker lock(&m_mutex);
    return std::chrono::milliseconds(m_timer.elapsed());
}

bool ElapsedTimerThreadSafe::hasExpired(std::chrono::milliseconds ms) const
{
    QnReadLocker lock(&m_mutex);
    return m_timer.hasExpired(ms.count());
}

} // namespace utils
} // namespace nx
