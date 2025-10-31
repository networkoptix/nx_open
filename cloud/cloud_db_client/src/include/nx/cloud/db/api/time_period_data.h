// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

namespace nx::cloud::db::api {

struct TimePeriod
{
    int64_t startTimeMs = 0;
    int64_t durationMs = 0;

    TimePeriod() = default;

    TimePeriod(int64_t startTimeMs, int64_t durationMs):
        startTimeMs(startTimeMs),
        durationMs(durationMs)
    {
    }

    bool operator==(const TimePeriod&) const = default;
};

using TimePeriodList = std::vector<TimePeriod>;


} // namespace nx::cloud::db::api
