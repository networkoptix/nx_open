#include "elapsed_timer_thread_safe.h"

#include <mutex>

namespace nx {
namespace utils {

void ElapsedTimerThreadSafe::start()
{
    std::lock_guard<mutex_type> lock(m_mutex);
    m_timer.start();
}

void ElapsedTimerThreadSafe::stop()
{
    std::lock_guard<mutex_type> lock(m_mutex);
    m_timer.invalidate();
}

std::chrono::milliseconds ElapsedTimerThreadSafe::elapsedSinceStart() const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return std::chrono::milliseconds(m_timer.isValid() ? m_timer.elapsed() : 0);
}

bool ElapsedTimerThreadSafe::hasExpiredSinceStart(std::chrono::milliseconds ms) const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return m_timer.isValid() && m_timer.hasExpired(ms.count());
}

// Further methods may be useful, but their usage was excised out the code after refactoring
bool ElapsedTimerThreadSafe::isStarted() const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return m_timer.isValid();
}

std::chrono::milliseconds ElapsedTimerThreadSafe::elapsed() const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return std::chrono::milliseconds(m_timer.elapsed());
}

bool ElapsedTimerThreadSafe::hasExpired(std::chrono::milliseconds ms) const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return m_timer.hasExpired(ms.count());
}

} // namespace utils
} // namespace nx
