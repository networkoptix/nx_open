
#include "system_utils.h"

#include <nx/utils/time.h>

float nx::utils::calculateSystemUsageFrequency(
    std::chrono::system_clock::time_point lastLoginTime,
    float currentUsageFrequency)
{
    constexpr float kBurnPeriod = 5.0;
    constexpr const int kSecondsPerDay = 60 * 60 * 24;

    const auto fullDaysSinceLastLogin =
        std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::utcTime() - lastLoginTime).count() / kSecondsPerDay;
    return std::max<float>(
        (1 - fullDaysSinceLastLogin / kBurnPeriod) * currentUsageFrequency, 0);
}
