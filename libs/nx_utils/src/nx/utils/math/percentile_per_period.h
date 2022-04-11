// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <optional>

#include "percentile.h"
#include "../time.h"

namespace nx::utils::math {

/**
 * Calculates a requested percentile of a number series over a fixed-duration sliding window.
 *
 * Uses multiple Percentile instances internally without performing deallocation/reallocation
 * of memory. Each Percentile has a different data collection time with some overlap, where
 * overlap = period / percentileCount. So for a period of 60 seconds and percentileCount of 2,
 * p1 will have data collection time from 0 to 60 seconds, when it will save its percentile value,
 * clear itself, restart its collection period, and report its cached result until 90 seconds.
 * Meanwhile, percentile 2 will have data collection time from
 * 30 seconds to 90 seconds. It will cache its percentile at 90 seconds, clear and restart
 * collection, and report cached result until 120 seconds. The percentiles alternate indefinitely.
 *
 * p#: internal percentile implementation: e.g. p1
 * C: collection period for a given percentile
 * R: report period for a given percentile
 * 0          30         60         90         120        150        ...
 * |----------|----------|----------|----------|----------|----------
 *   p1C                 | p1R      |
 *            | p2C                 | p2R      |
 *                       | p1C                 | p1R      |
 *                                  | p2C                 | p2R      |
 */
template<typename ValueType>
class PercentilePerPeriod
{
private:
    using time_point = std::chrono::steady_clock::time_point;
    struct Context
    {
        Percentile<ValueType> percentile;
        time_point collectionStartTime;
        time_point collectionEndTime;

        Context(double percentile) : percentile(percentile) {}
    };

public:
    PercentilePerPeriod(
        double percentile,
        std::chrono::milliseconds period,
        std::size_t percentileCount = 2)
        :
        m_period(period)
    {
        using namespace std::chrono;

        NX_ASSERT(m_period > milliseconds(2));
        if (m_period <= milliseconds(2))
            const_cast<milliseconds&>(m_period) = milliseconds(2);

        NX_ASSERT(percentileCount >= 2);
        if (percentileCount < 2)
            percentileCount = 2;

        NX_ASSERT(m_period / percentileCount > milliseconds::zero(),
            "collection period overlap is too small! period: %1, percentileCount: %2",
            m_period, percentileCount);
        if (m_period / percentileCount == milliseconds::zero())
            percentileCount = 2;

        for (std::size_t i = 0; i < percentileCount; ++i)
            m_percentiles.emplace_back(percentile);
    }

    void add(const ValueType& value)
    {
        const auto now = nx::utils::monotonicTime();

        clearExpiredPercentiles(now);

        for (auto& context: m_percentiles)
        {
            if (context.collectionStartTime <= now && now < context.collectionEndTime)
                context.percentile.add(value);
        }
    }

    ValueType get() const
    {
        const auto now = nx::utils::monotonicTime();

        const_cast<PercentilePerPeriod<ValueType>*>(this)->clearExpiredPercentiles(now);

        return m_cachedValue;
    }

private:
    void initialize(time_point now)
    {
        const auto overlap = m_period / m_percentiles.size();
        for (std::size_t i = 0; i < m_percentiles.size(); ++i)
        {
            auto& context = m_percentiles[i];
            context.collectionStartTime = now + overlap * i;
            context.collectionEndTime = context.collectionStartTime + m_period;
            context.percentile.clear();
        }

        m_cachedValue = ValueType{};
        m_earliest = 0;
    }

    void clearExpiredPercentiles(time_point now)
    {
        if (m_percentiles[m_earliest].collectionEndTime + m_period <= now
            || m_percentiles[m_earliest].collectionEndTime == time_point{})
        {
            // Too much time has passed for collection times to fix themselves.
            initialize(now);
            return;
        }

        for (std::size_t i = 0; i < m_percentiles.size(); ++i)
        {
            auto& context = m_percentiles[i];
            if (context.collectionEndTime <= now)
            {
                m_earliest = i == m_percentiles.size() - 1 ? 0 : i + 1;
                m_cachedValue = !context.percentile.empty()
                    ? context.percentile.get()
                    : ValueType{};
                context.percentile.clear();
                context.collectionStartTime = context.collectionEndTime;
                context.collectionEndTime = context.collectionStartTime + m_period;
            }
        }
    }

private:
    const std::chrono::milliseconds m_period;

    std::vector<Context> m_percentiles;
    ValueType m_cachedValue;
    std::size_t m_earliest{0};
};

} // namespace nx::utils::math
