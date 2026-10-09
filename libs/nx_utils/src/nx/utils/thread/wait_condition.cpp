// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "wait_condition.h"

WaitConditionTimer::WaitConditionTimer(
    nx::WaitCondition* waitCondition, std::chrono::milliseconds timeout):
    m_waitCondition(waitCondition),
    m_timeout(timeout),
    // Measured on the real clock: a test shifting nx::utils::monotonicTime() back while a wait is
    // pending would otherwise stretch the wait by the shift.
    m_startTime(std::chrono::steady_clock::now())
{
}

bool WaitConditionTimer::wait(nx::Mutex* mutex)
{
    using namespace std::chrono;

    if (m_timeout == std::chrono::milliseconds::max())
    {
        m_waitCondition->wait(mutex);
        return true;
    }

    const auto timePassed = steady_clock::now() - m_startTime;
    if (timePassed >= m_timeout)
        return false;

    return m_waitCondition->wait(
        mutex,
        duration_cast<milliseconds>(m_timeout - timePassed).count());
}
