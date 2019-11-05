#include "timed_guard.h"

namespace nx::vms::server::sdk_support {

TimedGuard::TimedGuard(std::chrono::milliseconds timeout, std::function<void()> onExpired)
{
    m_timer.start(timeout, std::move(onExpired));
}

TimedGuard::~TimedGuard()
{
    m_timer.pleaseStopSync();
}

} // namespace nx::vms::server::sdk_support
