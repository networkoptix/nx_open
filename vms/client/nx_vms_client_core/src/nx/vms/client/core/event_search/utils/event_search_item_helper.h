// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <utility>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/detail/predicate.h>
#include <recording/time_period.h>

#include "detail/item_helper_detail.h"

namespace nx::vms::client::core {

using namespace std::chrono;

/** Removes duplcated items from the data using id. Returns number of removed elemnts. */
template<typename Facade, typename Container, typename ItemIterator>
int removeDuplicateItems(Container& container,
    const ItemIterator& itBegin,
    const ItemIterator& itEnd)
{
    std::set<nx::Uuid> foundIds;
    const auto removeStartIt = std::remove_if(itBegin, itEnd,
        [&foundIds](const auto& item)
        {
            const auto id = Facade::id(item);
            if (foundIds.find(id) != foundIds.end())
                return true;

            foundIds.insert(id);
            return false;
        });

    container.erase(removeStartIt, itEnd);
    return std::distance(removeStartIt, itEnd);
}

/**
 * Merges old data (which can't be changed) and new one to one container. Removes intersected
 * elements. Old data is always sorted in descending order.
 */
template<typename Facade,
    typename NewDataContainer,
    typename OldDataContainer>
void mergeOldData(NewDataContainer& newData,
    const OldDataContainer& oldData,
    EventSearch::FetchDirection direction)
{
    NX_ASSERT(detail::isSortedCorrectly<Facade>(oldData, Qt::DescendingOrder));
    NX_ASSERT(detail::isSortedCorrectly<Facade>(newData, sortOrderFromDirection(direction)));

    detail::printDebugData<Facade>(newData, "old data");

    if (oldData.empty())
        return; // Nothing to merge.

    if (newData.empty())
    {
        NewDataContainer data = direction == EventSearch::FetchDirection::newer
            ? NewDataContainer(oldData.rbegin(), oldData.rend())
            : NewDataContainer(oldData.begin(), oldData.end());
        std::swap(newData, data);
        return;
    }

    detail::printDebugData<Facade>(newData, "new data");

    NewDataContainer result;
    result.reserve(oldData.size() + newData.size());
    auto inserter = std::back_inserter(result);

    using Predicate = event_search::detail::Predicate<Facade>;
    const auto less = Predicate::less(direction);

    if (direction == EventSearch::FetchDirection::newer)
        std::merge(oldData.rbegin(), oldData.rend(), newData.begin(), newData.end(), inserter, less);
    else
        std::merge(oldData.begin(), oldData.end(), newData.begin(), newData.end(), inserter, less);

    NX_ASSERT(detail::isSortedCorrectly<Facade>(result, sortOrderFromDirection(direction)));
    detail::printDebugData<Facade>(result, "after merge");

    const auto removeDuplicates =
        [&result, direction](typename Facade::TimeType start, typename Facade::TimeType end)
        {
            if (direction == EventSearch::FetchDirection::older)
                std::swap(start, end);

            const auto startIt = std::lower_bound(
                result.begin(), result.end(), start, Predicate::lowerBound(direction));
            const auto endIt = std::upper_bound(
                result.begin(), result.end(), end, Predicate::upperBound(direction));
            return removeDuplicateItems<Facade>(result, startIt, endIt);
        };

    const auto oldDataStartTime = std::min(Facade::startTime(oldData.front()),
        Facade::startTime(oldData.back()));
    const auto oldDataEndTime = std::max(Facade::startTime(oldData.front()),
        Facade::startTime(oldData.back()));
    const auto newDataStartTime = std::min(Facade::startTime(newData.front()),
        Facade::startTime(newData.back()));
    const auto newDataEndTime = std::max(Facade::startTime(newData.front()),
        Facade::startTime(newData.back()));

    const auto intersectionStartTime = std::max(oldDataStartTime, newDataStartTime);
    const auto intersectionEndTime = std::min(oldDataEndTime, newDataEndTime);

    if (intersectionStartTime <= intersectionEndTime) //< Has time periods intersection.
    {
        removeDuplicates(intersectionStartTime, intersectionEndTime);
        detail::printDebugData<Facade>(result, "after normal deletion");
    }

    NX_ASSERT(detail::isSortedCorrectly<Facade>(result, sortOrderFromDirection(direction)));

    const bool hasRemovals = removeDuplicates(Facade::TimeType::zero(), Facade::TimeType::max());
    if (hasRemovals)
    {
        detail::printDebugData<Facade>(result, "wrong objects");
        NX_ASSERT(false, "Found duplicates in inappropriate stage of merging.");
    }
    std::swap(newData, result);
}

/**
 * Helper function to get bounding time period for the fetched data.
 */
template<typename Facade, typename Container>
OptionalTimePeriod timeWindow(const Container& container)
{
    if (container.empty())
        return {};

    const auto first = Facade::startTime(*container.begin());
    const auto last = Facade::startTime(*container.rbegin());

    // As some of the models work with microseconds (and time window is measured in milliseconds)
    // we need to round time window to the correct direction.
    const auto start = floor<milliseconds>(std::min(first, last));
    const auto end = ceil<milliseconds>(std::max(first, last));
    return QnTimePeriod::fromInterval(start, end);
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
