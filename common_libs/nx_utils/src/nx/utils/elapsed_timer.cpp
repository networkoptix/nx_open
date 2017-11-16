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

} // namespace utils
} // namespace nx
