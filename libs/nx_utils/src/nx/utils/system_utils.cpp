// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_utils.h"

#include <nx/utils/time.h>

using namespace std::chrono;

namespace nx::utils {

float calculateSystemUsageFrequency(
    system_clock::time_point lastLoginTime,
    float currentUsageFrequency)
{
    constexpr const int kSecondsPerDay = 60 * 60 * 24;

    const auto fullDaysSinceLastLogin =
        duration_cast<seconds>(
            utcTime() - lastLoginTime).count() / kSecondsPerDay;
    return std::max<float>(
        (1 - fullDaysSinceLastLogin / kSystemAccessBurnPeriodFullDays) * currentUsageFrequency, 0);
}

} // namespace nx::utils
