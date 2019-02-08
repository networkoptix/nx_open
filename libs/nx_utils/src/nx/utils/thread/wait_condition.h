#pragma once

#include "mutex.h"

// TODO: Remove with all usages.
using QnWaitCondition = nx::utils::WaitCondition;

class NX_UTILS_API WaitConditionTimer
{
public:
    WaitConditionTimer(
        QnWaitCondition* waitCondition,
        std::chrono::milliseconds timeout);

    /**
     * @return false if timeout has passed since object instantiation.
     */
    bool wait(QnMutex* mutex);

private:
    QnWaitCondition* m_waitCondition;
    const std::chrono::milliseconds m_timeout;
    const std::chrono::steady_clock::time_point m_startTime;
};
