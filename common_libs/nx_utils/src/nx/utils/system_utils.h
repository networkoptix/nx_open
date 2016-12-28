#pragma once

#include <chrono>

namespace nx {
namespace utils {

constexpr float kSystemAccessBurnPeriodFullDays = 5.0;

float NX_UTILS_API calculateSystemUsageFrequency(
    std::chrono::system_clock::time_point lastLoginTime,
    float currentUsageFrequency);

} // namespace utils
} // namespace nx
