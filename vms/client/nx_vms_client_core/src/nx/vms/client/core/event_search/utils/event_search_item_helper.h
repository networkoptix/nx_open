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

/** Removes duplicated items from the data using id. Returns number of removed elements. */
template<typename Facade, typename Container, typename ItemIterator>
int removeDuplicateItems(Container& container,
    const ItemIterator& itBegin,
    const ItemIterator& itEnd,
    EventSearch::FetchDirection direction)
{
    std::set<typename Facade::IdType> foundIds;
    const auto isDuplicate =
        [&foundIds](const auto& item)
        {
            const auto id = Facade::id(item);
            if (foundIds.find(id) != foundIds.end())
                return true;

            foundIds.insert(id);
            return false;
        };

    // If duplicates are encountered, we must keep events with older start time and remove events
    // with newer start time, such as Analytics object tracks received not from their beginning.

    // In case of `FetchDirection::older` the container is sorted new-to-old, therefore we must do
    // the duplication removal in reverse order.

    if (const bool reverse = direction == EventSearch::FetchDirection::older)
    {
        const auto revItBegin = std::make_reverse_iterator(itEnd);
        const auto revItEnd = std::make_reverse_iterator(itBegin);

        const auto removeStartRevIt = std::remove_if(revItBegin, revItEnd, isDuplicate);
        const auto count = std::distance(removeStartRevIt, revItEnd);
        container.erase(itBegin, removeStartRevIt.base());
        return count;
    }
    else
    {
        const auto removeStartIt = std::remove_if(itBegin, itEnd, isDuplicate);
        const auto count = std::distance(removeStartIt, itEnd);
        container.erase(removeStartIt, itEnd);
        return count;
    }
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
    detail::printDebugData<Facade>(oldData, "old data");

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

    // TODO: in case of vector search data is sorted by another condition: similarity, startTime
    // So far I assume we never merged data that found by vector search. So, moving asserts below should help.
    // As a better fix it is needed to update sorting predicates.
    NX_ASSERT(detail::isSortedCorrectly<Facade>(oldData, Qt::DescendingOrder));
    NX_ASSERT(detail::isSortedCorrectly<Facade>(newData, sortOrderFromDirection(direction)));

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

    // When newData and oldData overlap, in the merged result we get duplicates with the same ID
    // and the same start time.

    // Besides that, if `oldData` was (partially) received from live, and some prolonged event was
    // received not from its beginning (it can happen with Analytics object tracks), it can have
    // start time greater than its archived true start time. And we potentially could have received
    // that event in `newData` with its true start time. Such events appear as duplicates with the
    // same ID and different start time.

    removeDuplicateItems<Facade>(result, result.begin(), result.end(), direction);
    detail::printDebugData<Facade>(result, "after removal of duplicates");

    NX_ASSERT(detail::isSortedCorrectly<Facade>(result, sortOrderFromDirection(direction)));
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
 * to the specified direction. Emits all needed model signals.
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
    const auto getLogStr =
        [](const auto beginIt, const auto endIt)
        {
            QStringList result;
            for (auto it = beginIt; it != endIt; ++it)
            {
                result.push_back(nx::format("{\"id\":\"%1\",\"startTime\":%2}",
                    Facade::id(*it), Facade::startTime(*it).count()));
            }
            return nx::format("{%1}", result.join(","));
        };

    NX_TRACE(model, "*** Update data, direction=%1 ***\n"
        "Current data (%1):\n%2\n"
        "Fetched data (%3):\n%4\n"
        "Body offset: %5, length: %6\n"
        "Tail offset: %7, length: %8",
        current.size(), getLogStr(current.cbegin(), current.cend()),
        fetched.data.size(), getLogStr(fetched.data.cbegin(), fetched.data.cend()),
        fetched.ranges.body.offset, fetched.ranges.body.length,
        fetched.ranges.tail.offset, fetched.ranges.tail.length);

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
                    topRange.offset = 0; // Top range offset is always 0.
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
