// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <recording/time_period.h>

#include "data_list_traits.h"

namespace nx::vms::client::core {
namespace timeline {

/**
 * This is a set of helpers to work with abstract data lists sorted by a timestamp
 * in *descending* order. Timestamps are obtained via `DataListTraits<DataList>`.
 */
template<typename DataList>
struct DataListHelpers
{
    static std::chrono::milliseconds timestamp(const auto& item)
    {
        return DataListTraits<DataList>::timestamp(item);
    }

    static std::chrono::milliseconds timestamp(std::chrono::milliseconds source)
    {
        return source;
    }

    /** Ordering predicate. */
    static constexpr auto greater =
        [](const auto& left, const auto& right)
        {
            return timestamp(left) > timestamp(right);
        };

    static QnTimePeriod coveredPeriod(const DataList& data)
    {
        if (data.empty())
            return QnTimePeriod{};

        // Due to the sorting is descending order, the latest timestamp is the first in the list,
        // the earliest timestamp is the last in the list.
        // Add 1ms to the latest timestamp as `QnTimePeriod` is [startTime, endTime).
        return QnTimePeriod::fromInterval(
            timestamp(*std::prev(data.end())),
            timestamp(*data.begin()) + std::chrono::milliseconds(1));
    }

    /**
     * Returns a copy of the part of `data` with timestamps within `period`, with its length
     * limited to `limit` (`limit` <= 0 means no limit).
     */
    static DataList slice(const DataList& data, const QnTimePeriod& period, int limit = -1)
    {
        const auto beginIt = std::upper_bound(data.begin(), data.end(), period.endTime(), greater);
        auto endIt = std::upper_bound(data.begin(), data.end(), period.startTime(), greater);

        if (limit > 0)
        {
            const int length = (int) std::distance(beginIt, endIt);
            const int excess = length - limit;
            if (excess > 0)
                std::advance(endIt, -excess);
        }

        return DataList{beginIt, endIt};
    }

    /**
     * Removes from `data` part(s) with timestamps outside of `period`, and limits the remaining
     * length to `limit` (`limit` <= 0 means no limit).
     *
     * Equivalent to `data = slice(data, period, limit)`, but more memory effective.
     */
    static void clip(DataList& data, const QnTimePeriod& period, int limit = -1)
    {
        const auto beginIt = std::upper_bound(data.begin(), data.end(), period.endTime(), greater);
        const auto endIt = std::upper_bound(data.begin(), data.end(), period.startTime(), greater);

        int length = (int) std::distance(beginIt, endIt);
        if (limit > 0 && length > limit)
            length = limit;

        data.erase(data.begin(), beginIt);
        data.erase(data.begin() + length, data.end());
    }

    /** Appends `from` to the end of `to`. */
    static void append(DataList& to, DataList from)
    {
        to.insert(to.end(),
            std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()));
    }
};

} // namespace timeline
} // namespace nx::vms::client::core
