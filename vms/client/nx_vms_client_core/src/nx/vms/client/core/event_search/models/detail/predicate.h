// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/event_search/event_search_globals.h>

namespace nx::vms::client::core::event_search::detail {

template<typename Facade>
struct Predicate
{
    using Type = typename Facade::Type;
    using TimeType = typename Facade::TimeType;

    using LowerBoundPredicate = std::function<bool (const Type& left, TimeType right)>;
    using UpperBoundPredicate = std::function<bool (TimeType left, const Type& right)>;

    using FetchDirection = EventSearch::FetchDirection;

    static LowerBoundPredicate lowerBound(FetchDirection direction = FetchDirection::newer)
    {
        return direction == FetchDirection::newer
            ? [](const Type& lhs, TimeType rhs) { return Facade::startTime(lhs) < rhs; }
            : [](const Type& lhs, TimeType rhs) { return Facade::startTime(lhs) > rhs; };
    }

    static UpperBoundPredicate upperBound(FetchDirection direction = FetchDirection::newer)
    {
        return direction == FetchDirection::newer
            ? [](TimeType lhs, const Type& rhs) { return lhs < Facade::startTime(rhs); }
            : [](TimeType lhs, const Type& rhs) { return lhs > Facade::startTime(rhs); };
    }
};

} // namespace nx::vms::client::core::event_search::detail
