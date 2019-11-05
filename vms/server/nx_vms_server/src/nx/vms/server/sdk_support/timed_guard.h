#pragma once

#include <nx/network/aio/timer.h>

namespace nx::vms::server::sdk_support {

/**
 * This guard is supposed to be used in purposes of reacting to unusually long execution time of
 *     third-party code that we're unable to control. If timeout exceeds before the destructor is
 *     executed then `onExpired` callback is called. This is a pretty rough tool and thus should
 *     be used carefully since many factors (such as multithreading environment, CPU load, etc.)
 *     can affect execution time of a code block with such a guard.
 */
class TimedGuard
{
public:
    TimedGuard(std::chrono::milliseconds timeout, std::function<void()> onExpired);
    ~TimedGuard();

private:
    nx::network::aio::Timer m_timer;
};

} // namespace nx::vms::server::sdk_support
