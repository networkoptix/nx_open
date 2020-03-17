#include "elapsed_timer.h"

#include <nx/utils/time.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

using namespace std::chrono;
using namespace std::chrono_literals;

ElapsedTimer::ElapsedTimer(bool started)
{
    if (started)
        restart();
}

bool ElapsedTimer::hasExpired(milliseconds value) const
{
    if (!isValid())
        return true;

    return elapsed() > value;
}

milliseconds ElapsedTimer::restart()
{
    if (!m_lastRestartTime.has_value())
    {
        m_lastRestartTime = nx::utils::monotonicTime();
        return 0ms;
    }

    const auto previousTime = *m_lastRestartTime;
    m_lastRestartTime = nx::utils::monotonicTime();
    return duration_cast<milliseconds>(*m_lastRestartTime - previousTime);
}

void ElapsedTimer::invalidate()
{
    m_lastRestartTime.reset();
}

bool ElapsedTimer::isValid() const
{
    return m_lastRestartTime.has_value();
}

milliseconds ElapsedTimer::elapsed() const
{
    if (!NX_ASSERT(isValid()))
        return 0ms;
    return duration_cast<milliseconds>(nx::utils::monotonicTime() - *m_lastRestartTime);
}

int64_t ElapsedTimer::elapsedMs() const
{
    return duration_cast<milliseconds>(elapsed()).count();
}

// ----------------------------------------------- SafeElapsedTimer -------------------------------------------------------------

SafeElapsedTimer::SafeElapsedTimer(bool started): base_type(started)
{
}

bool SafeElapsedTimer::hasExpired(std::chrono::milliseconds value) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::hasExpired(value);
}

std::chrono::milliseconds SafeElapsedTimer::restart()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::restart();
}

void SafeElapsedTimer::invalidate()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    base_type::invalidate();
}

bool SafeElapsedTimer::isValid() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::isValid();
}

std::chrono::milliseconds SafeElapsedTimer::elapsed() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::elapsed();
}

int64_t SafeElapsedTimer::elapsedMs() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::elapsedMs();
}

} // namespace utils
} // namespace nx
