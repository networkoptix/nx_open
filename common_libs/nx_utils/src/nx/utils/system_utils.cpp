
#include "system_utils.h"

#include <nx/utils/time.h>

float nx::utils::calculateSystemUsageFrequency(
    std::chrono::system_clock::time_point lastLoginTime,
    float currentUsageFrequency)
{
    constexpr const int kSecondsPerDay = 60 * 60 * 24;

    const auto fullDaysSinceLastLogin =
        std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::utcTime() - lastLoginTime).count() / kSecondsPerDay;
    return std::max<float>(
        (1 - fullDaysSinceLastLogin / kSystemAccessBurnPeriodFullDays) * currentUsageFrequency, 0);
}
