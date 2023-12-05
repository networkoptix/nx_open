// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <analytics/db/analytics_db_types.h>

namespace nx::vms::client::core::detail {

struct ObjectTrackFacade
{
    using Type = nx::analytics::db::ObjectTrackEx;
    using TimeType = std::chrono::microseconds;

    static TimeType startTime(const Type& track);

    static const QnUuid& id(const Type& data);

    static bool equal(const Type& left, const Type& right);
};

} // namespace nx::vms::client::core::detail
