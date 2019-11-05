#pragma once

#include <nx/network/aio/timer.h>

namespace nx::utils {

class TimedGuard
{
public:
    TimedGuard(std::chrono::milliseconds timeout, std::function<void()> onExpired);
    ~TimedGuard();

private:
    nx::network::aio::Timer m_timer;
};

} // namespace nx::utils
