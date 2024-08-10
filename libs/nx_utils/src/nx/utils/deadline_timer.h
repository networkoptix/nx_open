// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/time.h>

namespace nx::utils {

/**
 * Calculates future deadlines and verifies whether the deadline has expired. Uses special timer
 * that continues to run even if the system is suspended.
 * NOTE: hasExpired() returns false just after construction. So, setRemainingTime() has to be
 * invoked.
 */
class DeadlineTimer
{
public:
    bool hasExpired() const
    {
        if (!m_deadline)
            return false;

        return nx::utils::systemUptime() >= *m_deadline;
    }

    void setRemainingTime(std::chrono::milliseconds value)
    {
        m_deadline = nx::utils::systemUptime() + value;
    }

    std::chrono::milliseconds remainingTime() const
    {
        if (!m_deadline)
            return std::chrono::milliseconds::max();

        return std::max(*m_deadline - nx::utils::systemUptime(), std::chrono::milliseconds::zero());
    }

private:
    std::optional<std::chrono::milliseconds> m_deadline;
};

} // namespace nx::utils
