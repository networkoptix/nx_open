// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <numeric>

#include <nx/utils/std/optional.h>

#include "../time.h"

namespace nx {
namespace utils {
namespace math {

/**
 * Given stream of values returns sum of values per specified period.
 * Every operation has constant-time complexity against number of values provided.
 * It is achieved by calculating sums for a number of subperiods
 * and summing them up when needed.
 * Such approach results in calculation error of 1.0 / {sub period count}.
 * So, this class is effective for cases with a very large number of values.
 * E.g., calculating traffic per period.
 * Max error can be retrieved with SumPerPeriod::maxError().
 */
template<typename Value>
class SumPerPeriod
{
public:
    SumPerPeriod(
        std::chrono::milliseconds period,
        int subPeriodCount = 20)
        :
        m_subPeriodCount(subPeriodCount),
        m_subperiod(
            std::chrono::duration_cast<std::chrono::microseconds>(period) / m_subPeriodCount)
    {
        m_sumPerSubperiod.resize(m_subPeriodCount);
    }

    void add(Value value)
    {
        const auto now = monotonicTime();
        closeExpiredPeriods(now);
        m_sumPerSubperiod.front() += value;
        m_currentSum += value;
    }

    /**
     * Equally distributes value over last valuePeriod.
     */
    void add(Value value, std::chrono::microseconds valuePeriod)
    {
        using namespace std::chrono;

        if (valuePeriod == std::chrono::microseconds::zero())
            return add(value); //< Instant value.

        const auto now = monotonicTime();
        closeExpiredPeriods(now);

        for (int periodIndex = 0; periodIndex < m_subPeriodCount; ++periodIndex)
        {
            const auto currentPeriodLength =
                periodIndex == 0
                ? duration_cast<microseconds>(now - *m_currentPeriodStart)
                : m_subperiod;
            if (currentPeriodLength >= valuePeriod)
            {
                m_sumPerSubperiod[periodIndex] += value;
                m_currentSum += value;
                return;
            }

            const auto valuePerCurrentPeriod =
                (Value) (value * ((double) currentPeriodLength.count() / valuePeriod.count()));
            m_sumPerSubperiod[periodIndex] += valuePerCurrentPeriod;
            m_currentSum += valuePerCurrentPeriod;
            value -= valuePerCurrentPeriod;
            valuePeriod -= currentPeriodLength;
        }

        // Ignoring value part that corresponds to period older than oldest sub period.
    }

    Value getSumPerLastPeriod() const
    {
        const_cast<SumPerPeriod*>(this)->closeExpiredPeriods(monotonicTime());
        return m_currentSum;
    }

    float maxError() const
    {
        return 1.0 / m_subPeriodCount;
    }

    void reset()
    {
        m_sumPerSubperiod = std::deque<Value>(m_subPeriodCount);
        m_currentSum = Value();
        m_currentPeriodStart = std::nullopt;
    }

private:
    const int m_subPeriodCount;
    const std::chrono::microseconds m_subperiod;
    // TODO: #akolesnikov Use cyclic array here.
    std::deque<Value> m_sumPerSubperiod;
    Value m_currentSum = Value();
    std::optional<std::chrono::steady_clock::time_point> m_currentPeriodStart;

    void closeExpiredPeriods(
        std::chrono::steady_clock::time_point now)
    {
        if (!m_currentPeriodStart)
        {
            m_currentPeriodStart = now;
            return;
        }

        // TODO: #akolesnikov The following loop can be greatly simplified
        // if add expiration time to m_sumPerSubperiod.

        const auto deadline = now - m_subperiod * m_subPeriodCount;
        for (int i = 0; i < m_subPeriodCount; ++i)
        {
            const auto lastPeriodStart =
                *m_currentPeriodStart - (m_subPeriodCount - 1) * m_subperiod;
            const auto lastPeriodEnd = lastPeriodStart + m_subperiod;
            if (lastPeriodEnd > deadline)
            {
                break;
            }
            else
            {
                m_currentSum -= m_sumPerSubperiod.back();
                m_sumPerSubperiod.pop_back();
                m_sumPerSubperiod.push_front(Value());
                *m_currentPeriodStart += m_subperiod;
            }
        }
    }
};

//-------------------------------------------------------------------------------------------------

template<typename Value>
class SumPerMinute:
    public SumPerPeriod<Value>
{
    using base_type = SumPerPeriod<Value>;

public:
    SumPerMinute(int subPeriodCount = 20):
        base_type(std::chrono::minutes(1), subPeriodCount)
    {
    }
};

} // namespace math
} // namespace utils
} // namespace nx
