// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/time_period_data.h>

namespace nx::cloud::db::client {

template <typename ListType>
ListType uncompressTimePeriods(const std::vector<int64_t>& compressed, const bool ascOrder, int multiplier = 1)
{
    using value_t = typename ListType::value_type;
    ListType result(compressed.size() / 2);
    int64_t current = 0;
    if (ascOrder)
    {
        for (size_t i = 0; i < compressed.size() / 2; ++i)
        {
            current += compressed[i * 2] * multiplier;
            const auto durationMs = compressed[i * 2 + 1] * multiplier;
            result[i] = value_t(current, durationMs);
            current += durationMs;
        }
    }
    else
    {
        for (size_t i = 0; i < compressed.size() / 2; ++i)
        {
            current = std::abs(current - compressed[i * 2] * multiplier); //< Point to a period end time.
            const auto durationMs = compressed[i * 2 + 1] * multiplier;
            current -= durationMs; //< Point to a period start time.
            result[i] = value_t(current, durationMs);
        }
    }
    return result;
}

} // namespace nx::cloud::db::client
