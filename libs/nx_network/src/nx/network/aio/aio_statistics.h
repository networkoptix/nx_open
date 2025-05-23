// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::network::aio {

struct AioStatistics
{
    struct AioThreadData
    {
        size_t queueSize = 0;
        std::chrono::microseconds averageExecutionTimeUsecPerLastPeriod;
        std::chrono::milliseconds periodMsec;
        std::optional<bool> stuck;
    };

    struct StuckAioThreadData
    {
        int count = 0;
        std::vector<uintptr_t> threadIds;
        int averageTimeMultiplier;
        std::chrono::milliseconds absoluteThresholdMsec;
    };

    int threadCount = 0;
    size_t queueSizesTotal = 0;
    std::map<uintptr_t, AioThreadData> threads;
    std::optional<StuckAioThreadData> stuckThreadData;
};

NX_REFLECTION_INSTRUMENT(
    AioStatistics::AioThreadData,
    (queueSize)(averageExecutionTimeUsecPerLastPeriod)(periodMsec)(stuck))

NX_REFLECTION_INSTRUMENT(
    AioStatistics::StuckAioThreadData,
    (count)(threadIds)(averageTimeMultiplier)(absoluteThresholdMsec))

NX_REFLECTION_INSTRUMENT(
    AioStatistics,
    (threadCount)(queueSizesTotal)(threads)(stuckThreadData))

} // namespace nx::network::aio
