#include "elapsed_timer.h"

namespace nx {
namespace utils {

ElapsedTimer::ElapsedTimer()
{
    m_timer.invalidate();
}

bool ElapsedTimer::hasExpired(std::chrono::milliseconds value) const
{
    return !m_timer.isValid() || m_timer.hasExpired(value.count());
}

void ElapsedTimer::restart()
{
    m_timer.restart();
}

void ElapsedTimer::invalidate()
{
    m_timer.invalidate();
}

bool ElapsedTimer::isValid() const
{
    return m_timer.isValid();
}

std::chrono::milliseconds ElapsedTimer::elapsed() const
{
    return std::chrono::milliseconds(m_timer.elapsed());
}

qint64 ElapsedTimer::elapsedMs() const
{
    return m_timer.elapsed();
}

} // namespace utils
} // namespace nx
