// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/event_search/models/fetched_data.h>

namespace nx::vms::client::core::detail {

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
    auto fetchedBegin = fetched.data.begin() + fetched.ranges.body.offset;
    auto fetchedEnd = fetched.data.begin() + fetched.ranges.body.offset
        + fetched.ranges.body.length;

    const auto removeItems =
        [model, &old](auto begin, auto end)
    {
        const int start = std::distance(old.begin(), begin);
        const int count = std::distance(begin, end);
        const typename Model::ScopedRemoveRows removeRows(model, start, start + count - 1);
        return old.erase(begin, end);
    };

    const auto insertItems =
        [model, &old](auto position, auto begin, auto end)
    {
        const int start = std::distance(old.begin(), position);
        const int count = std::distance(begin, end);
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
            return;
        }

        if (fetchedBegin == fetchedEnd)
        {
            // Remove all remaining current items.
            removeItems(itOld, old.end());
            return;
        }

               // We suppose there are not a lot items with the same timestamp. So to avoid redundant
               // operations it was decided to develop it with step-by-step (with while loop)
               // implementation rather than using lower/uppper_bound/find versions.

               // Remove all items "above" current fetched items.
        auto itRemoveEnd = itOld;
        while (Accessor::startTime(*itRemoveEnd) > Accessor::startTime(*fetchedBegin)
            && itRemoveEnd != old.end())
        {
            ++itRemoveEnd;
        }

        if (itOld != itRemoveEnd)
        {
            itOld = removeItems(itOld, itRemoveEnd);
            if (itOld == old.end())
                continue;
        }

               // Insert all items "above" current old items.
        auto itInsertEnd = fetchedBegin;
        while (Accessor::startTime(*itOld) < Accessor::startTime(*itInsertEnd))
            ++itInsertEnd;
        if (fetchedBegin != itInsertEnd)
        {
            itOld = insertItems(itOld, fetchedBegin, itInsertEnd);
            fetchedBegin = itInsertEnd;
            if (itOld == old.end() || fetchedBegin == fetchedEnd)
                continue;
        }

               // Process "same timestamp" bookmarks
        if (!NX_ASSERT(Accessor::startTime(*itOld) == Accessor::startTime(*fetchedBegin)))
            continue;

        const auto currentTimestamp = Accessor::startTime(*itOld);
        while (itOld != old.end() && fetchedBegin != fetchedEnd
            && Accessor::startTime(*itOld) == currentTimestamp)
        {
            auto fetchedIt = fetchedBegin;
            while (fetchedIt != fetchedEnd && Accessor::startTime(*fetchedIt) == currentTimestamp
                && Accessor::id(*itOld) != Accessor::id(*fetchedIt))
            {
                ++fetchedIt;
            }

            if (fetchedIt == fetchedEnd || Accessor::startTime(*fetchedIt) != currentTimestamp)
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
