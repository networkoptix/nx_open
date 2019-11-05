#include "timed_guard.h"

namespace nx::vms::server::sdk_support {

/**
 * This guard is supposed to be used in purposes of reacting to unusually long execution time of
 *     third-party code that we're unable to control. If timeout exceeds before the destructor is
 *     executed then `onExpired` callback is called. This is a pretty rough tool and thus should
 *     be used carefully since many factors (such as multithreading environment, CPU load, etc.)
 *     can affect execution time of a code block with such a guard.
 */
TimedGuard::TimedGuard(std::chrono::milliseconds timeout, std::function<void()> onExpired)
{
    m_timer.start(timeout, std::move(onExpired));
}

TimedGuard::~TimedGuard()
{
    m_timer.pleaseStopSync();
}

} // namespace nx::vms::server::sdk_support
