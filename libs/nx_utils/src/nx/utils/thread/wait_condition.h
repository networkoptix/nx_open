// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "mutex.h"

class NX_UTILS_API WaitConditionTimer
{
public:
    WaitConditionTimer(
        nx::WaitCondition* waitCondition,
        std::chrono::milliseconds timeout);

    /**
     * @return false if timeout has passed since object instantiation.
     */
    bool wait(nx::Mutex* mutex);

private:
    nx::WaitCondition* m_waitCondition;
    const std::chrono::milliseconds m_timeout;
    const std::chrono::steady_clock::time_point m_startTime;
};
