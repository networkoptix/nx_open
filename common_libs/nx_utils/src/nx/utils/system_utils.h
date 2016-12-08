
#pragma once

#include <chrono>

namespace nx
{
namespace utils
{

float NX_UTILS_API calculateSystemUsageFrequency(
    std::chrono::system_clock::time_point lastLoginTime,
    float currentUsageFrequency);

} // namespace utils
} // namespace nx