// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <utility>

#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/detail/predicate.h>
#include <recording/time_period.h>

#include "detail/item_helper_detail.h"

namespace nx::vms::client::core {

using namespace std::chrono;

/**
 * Helper function to get bounding time period for the fetched data.
 */
template<typename Facade, typename Container>
OptionalTimePeriod timeWindow(const Container& container)
{
    if (container.empty())
        return {};

    const auto first = duration_cast<milliseconds>(Facade::startTime(*container.begin()));
    const auto last = duration_cast<milliseconds>(Facade::startTime(*container.rbegin()));
    return QnTimePeriod::fromInterval(std::min(first, last), std::max(first, last));
}

/**
 * Updates current data by a new one. Updates body items and inserts tail in the position according
 * to the specifeid direction. Emits all needed model signals.
 */
template<typename Facade,
    typename DataContainer,
    typename FetchedData,
    typename Model>
void updateEventSearchData(Model* model,
    DataContainer& current,
    FetchedData& fetched,
    EventSearch::FetchDirection direction)
{
    detail::updateBody<Facade>(model, current, fetched);
    detail::insertTail(model, current, fetched, direction);
}

template<typename Facade, typename Container>
void truncateRawData(Container& data,
    EventSearch::FetchDirection direction,
    typename Facade::TimeType maxTimestamp)
{
    const auto predicate = event_search::detail::Predicate<Facade>::lowerBound(
        EventSearch::FetchDirection::older);
    if (direction == EventSearch::FetchDirection::older)
    {

        const auto end = std::lower_bound(data.begin(), data.end(), maxTimestamp, predicate);
        data.erase(data.begin(), end);
    }
    else if (NX_ASSERT(direction == EventSearch::FetchDirection::newer))
    {
        const auto begin = std::lower_bound(data.rbegin(), data.rend(), maxTimestamp,
            predicate).base();
        data.erase(begin, data.end());
    }
}

/**
 * Truncates final fetched data according to the specified direction to fit target size.
 */
template<typename Data>
void truncateFetchedData(
    Data& fetched,
    EventSearch::FetchDirection direction,
    int targetSize)
{
    if (int(fetched.data.size()) <= targetSize)
        return;

    const int maxTailSize = targetSize / 2;

    using Range = FetchedDataRanges::ItemsRange;
    const auto cutBack =
        [&fetched, direction](int toRemove)
        {
            const int newCount = int(fetched.data.size()) - toRemove;
            fetched.data.erase(fetched.data.begin() + newCount, fetched.data.end());
            const auto cutRanges =
                [newCount, toRemove](Range& topRange, Range& bottomRange)
                {
                    topRange.offset = 0; // Top ranfe offset is always 0.
                    topRange.length = std::min(topRange.length, newCount);

                    bottomRange.offset = std::min(bottomRange.offset, newCount);
                    bottomRange.length = std::max(0, bottomRange.length - toRemove);
                };

            if (direction == EventSearch::FetchDirection::newer)
                cutRanges(fetched.ranges.tail, fetched.ranges.body);
            else
                cutRanges(fetched.ranges.body, fetched.ranges.tail);
        };

    const auto cutFront =
        [&fetched, direction](int toRemove)
        {
            fetched.data.erase(fetched.data.begin(), fetched.data.begin() + toRemove);

            const auto shiftLeft =
                [toRemove](Range& topRange, Range& bottomRange)
                {
                    topRange.offset = 0; // Top range is always 0
                    topRange.length = std::max(0, topRange.length - toRemove);
                    bottomRange.length -= std::max(toRemove - bottomRange.offset, 0);
                    bottomRange.offset = std::max(0, bottomRange.offset - toRemove);
                };

            if (direction == EventSearch::FetchDirection::newer)
                shiftLeft(fetched.ranges.tail, fetched.ranges.body);
            else
                shiftLeft(fetched.ranges.body, fetched.ranges.tail);
        };

    // Check tail length.
    const int extraTailLength = fetched.ranges.tail.length - maxTailSize;
    if (extraTailLength > 0)
    {
        if (direction == EventSearch::FetchDirection::older)
            cutBack(extraTailLength);
        else
            cutFront(extraTailLength);
    }

    const auto removeCount = int(fetched.data.size()) - targetSize;
    if (removeCount <= 0)
        return;

    if (direction == EventSearch::FetchDirection::older)
        cutFront(removeCount);
    else
        cutBack(removeCount);
}

} // namespace nx::vms::client::core
