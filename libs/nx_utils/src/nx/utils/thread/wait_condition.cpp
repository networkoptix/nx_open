#include "wait_condition.h"

#include <nx/utils/time.h>

WaitConditionTimer::WaitConditionTimer(
    QnWaitCondition* waitCondition,
    std::chrono::milliseconds timeout)
    :
    m_waitCondition(waitCondition),
    m_timeout(timeout),
    m_startTime(nx::utils::monotonicTime())
{
}

bool WaitConditionTimer::wait(QnMutex* mutex)
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
