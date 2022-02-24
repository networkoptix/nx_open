// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include "sum_per_period.h"

namespace nx::utils::math {

/**
 * Helps to limit rate of some requests per period.
 * Internally, calculates number of approved requests per last period and
 * compares it against the allowed amount.
 * Note that it does not tend to distribute requests over the period evenly.
 * E.g., with a period of 1 hour it can allow all requests within the first second and then just
 * wait refuse everything to the next hour.
 * To achieve a more or less even distribution consider using smaller periods.
 * Period as small as 1 millisecond is normal since internally resolution of 1 microsecond is used.
 * So, to limit rps rate in a high-loaded system to 1000 rps it may be best to use period of
 * 1 millisecond and allowed amount of 1 in comparison to period of 1 second and amount of 1000.
 * Note: it uses nx::utils::math::SumPerPeriod internally. So, there is some error when calcuating
 * number of requests per current period. See nx::utils::math::SumPerPeriod for more details.
 */
class NX_UTILS_API RateLimiter
{
public:
    /**
     * @param subPeriodCount This value is passed to the nx::utils::math::SumPerPeriod class
     * which is used to calculate number of requests per period.
     * See nx::utils::math::SumPerPeriod for more details.
     */
    RateLimiter(
        std::chrono::milliseconds period,
        int amountAllowed,
        int subPeriodCount = 20);

    /**
     * Returns true if one more operation does not exceed the rate allowed.
     * false otherwise.
     * In case when true is returned the current rate is updated.
     * Run-time complexity is O(1).
     */
    bool isAllowed();

private:
    const int m_amountAllowed;
    nx::utils::math::SumPerPeriod<int> m_amountPerLastPeriod;
};

} // namespace nx::utils::math
