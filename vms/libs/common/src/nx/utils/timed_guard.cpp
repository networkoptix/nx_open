#include "timed_guard.h"

namespace nx::utils {

TimedGuard::TimedGuard(std::chrono::milliseconds timeout, std::function<void()> onExpired)
{
    m_timer.start(timeout, std::move(onExpired));
}

TimedGuard::~TimedGuard()
{
    m_timer.cancelSync();
}

} // namespace nx::utils
