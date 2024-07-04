// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

using namespace std::chrono_literals;

struct NX_VMS_API ServerTimePeriod
{
    /**
     * Whether this is an infinite time period.
     */
    constexpr bool isInfinite() const { return !durationMs.has_value(); }

    /**
     * Whether this is a null time period.
     */
    constexpr bool isNull() const { return startTimeMs == 0ms && durationMs == 0ms; };

    constexpr bool operator==(const ServerTimePeriod& other) const = default;

    std::chrono::milliseconds startTime() const;

    std::chrono::milliseconds endTime() const;
    void setEndTime(const std::chrono::milliseconds& value);

    void setDuration(const std::chrono::milliseconds& value);
    std::chrono::milliseconds duration() const;
    void resetDuration();

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    nx::Uuid serverId;

    /**%apidoc[opt] Start time in milliseconds. */
    std::chrono::milliseconds startTimeMs = kMinTimeValue;

    /**%apidoc
     * Duration in milliseconds. Not presented for a video chunk that is currently being recorded.
     */
    std::optional<std::chrono::milliseconds> durationMs;

    static constexpr auto kMinTimeValue = std::chrono::milliseconds::zero();

    static constexpr auto kMaxTimeValue =
        std::chrono::milliseconds(std::numeric_limits<qint64>::max());

    static ServerTimePeriod infinite();
};

using ServerTimePeriodList = std::vector<ServerTimePeriod>;
using DeviceFootageMap = std::map<nx::Uuid, ServerTimePeriodList>;

constexpr bool operator<(std::chrono::milliseconds timeMs, const ServerTimePeriod& other)
{
    return timeMs < other.startTimeMs;
};

#define ServerTimePeriod_Fields (serverId)(startTimeMs)(durationMs)

QN_FUSION_DECLARE_FUNCTIONS(ServerTimePeriod, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(ServerTimePeriod, ServerTimePeriod_Fields)

} // namespace nx::vms::api
