// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rate_limiter.h"

namespace nx::utils::math {

RateLimiter::RateLimiter(
    std::chrono::milliseconds period,
    int amountAllowed,
    int subPeriodCount)
    :
    m_amountAllowed(amountAllowed),
    m_amountPerLastPeriod(period, subPeriodCount)
{
}

bool RateLimiter::isAllowed()
{
    if (m_amountPerLastPeriod.getSumPerLastPeriod() >= m_amountAllowed)
        return false;
    m_amountPerLastPeriod.add(1);
    return true;
}

} // namespace nx::utils::math
