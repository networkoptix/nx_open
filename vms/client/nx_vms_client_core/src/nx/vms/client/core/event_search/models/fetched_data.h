// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/event_search/models/fetch_request.h>

#include "detail/predicate.h"

namespace nx::vms::client::core {

struct FetchedDataRanges
{
    struct ItemsRange
    {
        int offset = 0;
        int length = 0;
    };

    ItemsRange body; //< Body parameters.
    ItemsRange tail; //< Tail parameters.
};

/**
 * Structure to hold newly fetched event search data and body/tail markups. Data sorted in
 * descending order by timestamp.
 */
template<typename Container>
struct FetchedData
{
    using FetchedContainer = Container;

    Container data;
    FetchedDataRanges ranges;
};

/**
 * Current data is supposed to be sorted in descending order by start time.
 * Fetched data is supposed to be sorted by the start time in the descending order for the "older"
 * request direction and in the ascending order by the "newer" direction.
 * Algorithm supposes that we can't move items from one to another time, it supports only deletions
 * and insertions of newly created items.
 */
// Does not suppose to have moved by time items.
template<typename Facade,
    typename Container,
    typename CurrentContainer>
FetchedData<Container> makeFetchedData(
    const CurrentContainer& current,
    Container& fetched,
    const FetchRequest& request);

/** Details section. */

namespace detail {

template<typename Facade, typename Iterator>
bool isSortedCorrectly(const Iterator begin,
    const Iterator end,
    Qt::SortOrder order)
{
    using PredicateType = std::function<bool (const typename Facade::Type& lhs,
        const typename Facade::Type& rhs)>;

    const auto predicate = order == Qt::AscendingOrder
        ? PredicateType([](const auto& lhs, const auto& rhs)
            { return Facade::startTime(lhs) < Facade::startTime(rhs); })
        : PredicateType([](const auto& lhs, const auto& rhs)
            { return Facade::startTime(lhs) > Facade::startTime(rhs); });

    return std::is_sorted(begin, end, predicate);
}

template<typename Facade, typename Container>
bool isSortedCorrectly(const Container& container, Qt::SortOrder order)
{
    return isSortedCorrectly<Facade>(container.begin(), container.end(), order);
}

/*
 * To find out new existing central point we look for the central point both in the old and new
 * data. To use standard algorithms we need to use reversed data.
 */
template<typename Container,
    typename Facade,
    typename CurrentIterator,
    typename FetchedIterator>
FetchedData<Container> makeFetchedDataInternal(
    const CurrentIterator& currentBeginIt,
    const CurrentIterator& currentEndIt,
    const FetchedIterator& fetchedBeginIt,
    const FetchedIterator& fetchedEndIt,
    const FetchRequest& request)
{
    using ResultType = FetchedData<Container>;
    using Predicate = event_search::detail::Predicate<Facade>;

    const auto makeResult =
        [fetchedBeginIt, fetchedEndIt, request](const FetchedIterator& tailEnd)
    {
        const auto length = std::distance(fetchedBeginIt, fetchedEndIt);

        FetchedData<Container> result;
        result.ranges.tail.length = std::distance(fetchedBeginIt, tailEnd);
        result.ranges.body.length = length - result.ranges.tail.length;

        if (request.direction == EventSearch::FetchDirection::newer)
        {
            result.ranges.body.offset = result.ranges.tail.length;
            result.data = Container(fetchedBeginIt, fetchedEndIt);
        }
        else
        {
            result.ranges.tail.offset = length - result.ranges.tail.length;
            result.data.resize(length);
            int offset = length;
            for (auto it = fetchedBeginIt; it != fetchedEndIt; ++it)
                result.data[--offset] = *it;
        }
        return result;
    };

    if (currentBeginIt == currentEndIt || fetchedBeginIt == fetchedEndIt)
        return makeResult(fetchedEndIt);

    //As we use reversed data, we have to use opposite direction predicate
    const auto oppositeDirection = request.direction == EventSearch::FetchDirection::newer
        ? EventSearch::FetchDirection::older
        : EventSearch::FetchDirection::newer;
    const auto lowerBound = Predicate::lowerBound(oppositeDirection);
    const auto upperBound = Predicate::upperBound(oppositeDirection);
    auto fetchedRefTimeIt = fetchedBeginIt;
    auto currentRefTimeIt = currentBeginIt;
    auto centralPoint = duration_cast<typename Facade::TimeType>(request.centralPointUs);

    // Looking for the new real central point - for exmaple in case if initial item with central
    // point was deleted or moved somewhere. We have to find same items near the central point
    // (or find a new one) in both fetched and current data - that's gonna be a new central point.
    bool exactCentralPointExists = false;
    do
    {
        // Looking for the nearest time around central time point in fetched data.
        fetchedRefTimeIt = std::lower_bound(
            fetchedRefTimeIt, fetchedEndIt, centralPoint, lowerBound);
        if (fetchedRefTimeIt == fetchedEndIt)
        {
            // All fetched data is out of the bounds of current data from the requested direction
            // side (i.e. for FetchDirection::older all fetched data is older than the current one,
            // for FetchDirection::newer all fetched data is newer than the current one.
            // In this case we assume that all new data is a tail.
            break;
        }

        // New tail point if a found one.
        centralPoint = Facade::startTime(*fetchedRefTimeIt);

        // Looking for the item with the nearest tail time point in the current data.
        currentRefTimeIt = std::lower_bound(
            currentRefTimeIt, currentEndIt, centralPoint, lowerBound);
        if (currentRefTimeIt == currentEndIt)
        {
            // All fetched data is out of the bounds of current data from the direction which is
            // opposite to the requested (i.e. for FetchDirection::older all fetched data is newer
            // than the current one, for FetchDirection::newer all fetched data is older than the
            // current one.
            break;
        }

        if (Facade::startTime(*currentRefTimeIt) == centralPoint)
        {
            // We have some items with the same timepoints here both in current and fetched data.

            // Check if we have at least one "old" item in the fetched data.
            const auto currentRefEndIt = std::upper_bound(currentRefTimeIt, currentEndIt,
                centralPoint, upperBound);
            const auto fetchedReferenceTimeEndIt = std::upper_bound(fetchedRefTimeIt,
                fetchedEndIt, centralPoint, upperBound);
            const bool hasSameItem = std::any_of(currentRefTimeIt, currentRefEndIt,
                [fetchedRefTimeIt, fetchedReferenceTimeEndIt](const auto& item)
                {
                    const auto it = std::find_if(fetchedRefTimeIt,
                        fetchedReferenceTimeEndIt,
                        [&item](const auto& other)
                        {
                            return Facade::id(item) == Facade::id(other);
                        });
                    return it != fetchedReferenceTimeEndIt;
                });

            if (hasSameItem)
            {
                // Reference point item (and timestamp) is found.
                exactCentralPointExists = true;
                break;
            }

            // Looking for the next potential reference time point using current data.
            currentRefTimeIt = std::upper_bound(currentRefTimeIt, currentEndIt,
                centralPoint, upperBound);

            if (currentRefTimeIt == currentEndIt)
                break; //< Still no any "old" item in the fetched data.

            centralPoint = Facade::startTime(*currentRefTimeIt);

        }
        else
        {
            centralPoint = Facade::startTime(*currentRefTimeIt);
        }
    } while (true);

    if (!exactCentralPointExists)
    {
        // No central point was found - all the data is tail.
        return makeResult(fetchedEndIt);
    }

    // Move all "new" items with the reference timestamp to the correct side of the tail.
    const auto fetchedReferenceTimeEndIt = std::upper_bound(fetchedRefTimeIt,
        fetchedEndIt, centralPoint, upperBound);

    currentRefTimeIt = std::lower_bound(currentRefTimeIt, currentEndIt,
        centralPoint, lowerBound);
    const auto currentReferenceTimeEndIt = std::upper_bound(currentRefTimeIt, currentEndIt,
        centralPoint, upperBound);

    // Move existing items "lower" for the "newer" fetch dirction and "upper" for the "older" one.

    const auto moveToBackIf =
        [](auto begin, auto end, auto predicate)
    {
        while (begin != end)
        {
            if (predicate(*begin))
                std::swap(*begin, *--end);
            else
                ++begin;
        }
        return end;
    };

    const auto it = moveToBackIf(fetchedRefTimeIt, fetchedReferenceTimeEndIt,
        [currentRefTimeIt, currentReferenceTimeEndIt](const auto& item)
        {
            // We suppose that there are not a lot of items at the same milli(micro)second.
            const auto itSame = std::find_if(currentRefTimeIt, currentReferenceTimeEndIt,
                [&item](const auto& other)
                {
                    return Facade::id(item) == Facade::id(other);
                });

            return itSame != currentReferenceTimeEndIt;
        });

    return makeResult(it);
}

} // namespace detail

/** Implmentation section */
template<typename Facade,
    typename Container,
    typename CurrentContainer>
FetchedData<Container> makeFetchedData(
    const CurrentContainer& current,
    Container& fetched,
    const FetchRequest& request)
{
    using namespace EventSearch;
    NX_ASSERT(detail::isSortedCorrectly<Facade>(current, Qt::DescendingOrder));
    NX_ASSERT(detail::isSortedCorrectly<Facade>(fetched, sortOrderFromDirection(request.direction)));

    if (request.direction == EventSearch::FetchDirection::newer)
    {
        return detail::makeFetchedDataInternal<Container, Facade>(
            current.begin(), current.end(), fetched.rbegin(), fetched.rend(), request);
    }

    // Since we have different order for the "older" request we use reverse iterators here.
    return detail::makeFetchedDataInternal<Container, Facade>(
        current.rbegin(), current.rend(), fetched.rbegin(), fetched.rend(), request);
}

} // namespace nx::vms::client::core
