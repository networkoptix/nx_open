// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/event_search/event_search_globals.h>

namespace nx::vms::client::core::event_search::detail {

template<typename Facade>
struct Predicate
{
    using T = typename Facade::Type;
    using TimeType = typename Facade::TimeType;
    using LowerBoundPredicate = std::function<bool (const T& left, TimeType right)>;
    using UpperBoundPredicate = std::function<bool (TimeType left, const T& right)>;
    using LessPredicate = std::function<bool (const T& left, const T& right)>;
    using FetchDirection = EventSearch::FetchDirection;

    static LessPredicate less(FetchDirection direction = FetchDirection::newer)
    {
        return direction == FetchDirection::newer
            ? [](const T& lhs, const T& rhs) { return Facade::startTime(lhs) < Facade::startTime(rhs); }
            : [](const T& lhs, const T& rhs) { return Facade::startTime(lhs) > Facade::startTime(rhs); };
    }

    static LowerBoundPredicate lowerBound(FetchDirection direction = FetchDirection::newer)
    {
        return direction == FetchDirection::newer
            ? [](const T& lhs, TimeType rhs) { return Facade::startTime(lhs) < rhs; }
            : [](const T& lhs, TimeType rhs) { return Facade::startTime(lhs) > rhs; };
    }

    static UpperBoundPredicate upperBound(FetchDirection direction = FetchDirection::newer)
    {
        return direction == FetchDirection::newer
            ? [](TimeType lhs, const T& rhs) { return lhs < Facade::startTime(rhs); }
            : [](TimeType lhs, const T& rhs) { return lhs > Facade::startTime(rhs); };
    }
};

} // namespace nx::vms::client::core::event_search::detail
