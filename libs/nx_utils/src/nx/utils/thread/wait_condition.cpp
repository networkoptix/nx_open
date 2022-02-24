// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "wait_condition.h"

#include <nx/utils/time.h>

WaitConditionTimer::WaitConditionTimer(
    nx::WaitCondition* waitCondition,
    std::chrono::milliseconds timeout)
    :
    m_waitCondition(waitCondition),
    m_timeout(timeout),
    m_startTime(nx::utils::monotonicTime())
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

    const auto timePassed = nx::utils::monotonicTime() - m_startTime;
    if (timePassed >= m_timeout)
        return false;

    return m_waitCondition->wait(
        mutex,
        duration_cast<milliseconds>(m_timeout - timePassed).count());
}
