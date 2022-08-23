// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

namespace nx::utils {

constexpr float kSystemAccessBurnPeriodFullDays = 5.0;

float NX_UTILS_API calculateSystemUsageFrequency(
    std::chrono::system_clock::time_point lastLoginTime,
    float currentUsageFrequency);

} // namespace nx::utils
