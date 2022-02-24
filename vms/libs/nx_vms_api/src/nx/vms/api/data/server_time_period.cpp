// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_time_period.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/datetime.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerTimePeriod, (json), ServerTimePeriod_Fields)

static_assert(ServerTimePeriod::kMaxTimeValue.count() == DATETIME_NOW);

std::chrono::milliseconds ServerTimePeriod::startTime() const
{
    return startTimeMs;
}

std::chrono::milliseconds ServerTimePeriod::endTime() const
{
    if (isInfinite())
        return kMaxTimeValue;

    return startTimeMs + durationMs.value();
}

void ServerTimePeriod::setEndTime(const std::chrono::milliseconds& value)
{
    if (value != kMaxTimeValue)
        durationMs = std::max(kMinTimeValue, value - startTimeMs);
    else
        resetDuration();
}

void ServerTimePeriod::setDuration(const std::chrono::milliseconds& value)
{
    durationMs = value;
}

std::chrono::milliseconds ServerTimePeriod::duration() const
{
    return durationMs.value_or(std::chrono::milliseconds::min());
}

void ServerTimePeriod::resetDuration()
{
    durationMs.reset();
}

} // namespace nx::vms::api
