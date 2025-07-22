// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/event_search/models/fetched_data.h>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core::detail {

template<typename Facade, typename Container, typename Tag>
void printDebugData(const Container& container, const Tag& tag)
{
    NX_VERBOSE(NX_SCOPE_TAG, "Object debug data for '%1' start, size: %2", tag, container.size());
    for (const auto& item: container)
        NX_DEBUG(NX_SCOPE_TAG, "%1, %2", Facade::id(item), Facade::startTime(item).count());
    NX_DEBUG(NX_SCOPE_TAG, "Object debug data for '%1' end", tag);
}

/**
 * Inserts tail data to the current container according to the specified direction. Emits needed
 * model signals.
 */
template<typename DataContainer, typename FetchedData, typename Model>
void insertTail(
    Model* model,
    DataContainer& current,
    const FetchedData& fetched,
    EventSearch::FetchDirection direction)
{
    const auto fetchedBegin = fetched.data.begin() + fetched.ranges.tail.offset;
    const auto fetchedEnd = fetched.data.begin() + fetched.ranges.tail.offset
        + fetched.ranges.tail.length;
    const int count = std::distance(fetchedBegin, fetchedEnd);
    if (count <= 0)
        return;

    const int start = direction == EventSearch::FetchDirection::newer ? 0 : current.size();
    const typename Model::ScopedInsertRows insertRows(model, start, start + count - 1);
    current.insert(current.begin() + start, fetchedBegin, fetchedEnd);
}

/**
 * Updates current data by a new one. Inserts new items, removes deleted and updates the changed
 * ones. Emits all needed model signals.
 */
template<
    typename Accessor,
    typename OldDataContainer,
    typename FetchedData,
    typename Model>
void updateBody(
    Model* model,
    OldDataContainer& old,
    FetchedData& fetched)
{
    const auto removeItems =
        [model, &old](auto begin, auto end)
        {
            const int start = std::distance(old.begin(), begin);
            const int count = std::distance(begin, end);
            if (count <= 0)
                return end;

            const typename Model::ScopedRemoveRows removeRows(model, start, start + count - 1);
            return old.erase(begin, end);
        };

    if (fetched.data.empty())
    {
        removeItems(old.begin(), old.end());
        return;
    }

    auto fetchedBegin = fetched.data.begin() + fetched.ranges.body.offset;
    auto fetchedEnd = fetched.data.begin() + fetched.ranges.body.offset
        + fetched.ranges.body.length;

    const auto insertItems =
        [model, &old](auto position, auto begin, auto end)
        {
            const int start = std::distance(old.begin(), position);
            const int count = std::distance(begin, end);
            if (count <= 0)
                return position;

            const typename Model::ScopedInsertRows insertRows(model, start, start + count - 1);
            return old.insert(position, begin, end) + count;
        };

    const auto setData =
        [model, &old](auto position, auto value)
        {
            *position = value;
            const int row = std::distance(old.begin(), position);
            const auto index = model->index(row, 0);
            emit model->dataChanged(index, index);
        };

    auto itOld = old.begin();
    while (itOld != old.end() || fetchedBegin != fetchedEnd)
    {
        if (itOld == old.end())
        {
            // Insert all other fetched item to the end.
            insertItems(old.end(), fetchedBegin, fetchedEnd);
            break;
        }

        if (fetchedBegin == fetchedEnd)
        {
            // Remove all remaining current items.
            removeItems(itOld, old.end());
            break;
        }

        // We suppose there are not a lot items with the same timestamp. So to avoid redundant
        // operations it was decided to develop it with step-by-step (with while loop)
        // implementation rather than using lower/uppper_bound/find versions.

        // Remove all items "above" current fetched items.
        auto itRemoveEnd = itOld;
        auto nextTimestamp = Accessor::startTime(*fetchedBegin);

        while (itRemoveEnd != old.end() && Accessor::startTime(*itRemoveEnd) > nextTimestamp)
            ++itRemoveEnd;

        if (itOld != itRemoveEnd)
        {
            itOld = removeItems(itOld, itRemoveEnd);
            if (itOld == old.end())
                continue;
        }

        // Insert all items "above" current old items.
        auto itInsertEnd = fetchedBegin;
        nextTimestamp = Accessor::startTime(*itOld);

        while (itInsertEnd != fetchedEnd && Accessor::startTime(*itInsertEnd) > nextTimestamp)
            ++itInsertEnd;

        if (fetchedBegin != itInsertEnd)
        {
            itOld = insertItems(itOld, fetchedBegin, itInsertEnd);
            fetchedBegin = itInsertEnd;
            if (itOld == old.end() || fetchedBegin == fetchedEnd)
                continue;
        }

        if (Accessor::startTime(*itOld) != Accessor::startTime(*fetchedBegin))
        {
            NX_ASSERT(Accessor::startTime(*itOld) > Accessor::startTime(*fetchedBegin));
            continue;
        }

        // Process "same timestamp" bookmarks.

        nextTimestamp = Accessor::startTime(*itOld);
        while (itOld != old.end() && fetchedBegin != fetchedEnd
            && Accessor::startTime(*itOld) == nextTimestamp)
        {
            auto fetchedIt = fetchedBegin;
            while (fetchedIt != fetchedEnd && Accessor::startTime(*fetchedIt) == nextTimestamp
                && Accessor::id(*itOld) != Accessor::id(*fetchedIt))
            {
                ++fetchedIt;
            }

            if (fetchedIt == fetchedEnd || Accessor::startTime(*fetchedIt) != nextTimestamp)
            {
                // Didn't find old item in the fetched items with the same timestamp.
                itOld = removeItems(itOld, itOld + 1);
            }
            else
            {
                NX_ASSERT(Accessor::id(*fetchedIt) == Accessor::id(*itOld));
                if (!Accessor::equal(*fetchedIt, *itOld))
                    setData(itOld, *fetchedIt);

                std::swap(*fetchedIt, *fetchedBegin); //< Move up processed fetched item.
                ++fetchedBegin;
                ++itOld;
            }
        }
    }
}

} // namespace nx::vms::client::core::detail
