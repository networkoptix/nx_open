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
        m_subperiodCount(subPeriodCount),
        m_subperiod(
            std::chrono::duration_cast<std::chrono::microseconds>(period) / m_subperiodCount)
    {
        m_sumPerSubperiod.resize(m_subperiodCount);
    }

    std::chrono::microseconds subperiodLength() const { return m_subperiod; }

    void add(Value value)
    {
        const auto now = monotonicTime();
        closeExpiredPeriods(now);
        m_sumPerSubperiod[m_front] += value;
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

        for (int count = 0, i = m_front;
            count < m_subperiodCount;
            ++count, i = i == m_subperiodCount - 1 ? 0 : i + 1)
        {
            const auto currentPeriodLength =
                i == m_front
                ? duration_cast<microseconds>(now - *m_currentPeriodStart)
                : m_subperiod;
            if (currentPeriodLength >= valuePeriod)
            {
                m_sumPerSubperiod[i] += value;
                m_currentSum += value;
                return;
            }

            const auto valuePerCurrentPeriod =
                (Value) (value * ((double) currentPeriodLength.count() / valuePeriod.count()));
            m_sumPerSubperiod[i] += valuePerCurrentPeriod;
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
        return 1.0 / m_subperiodCount;
    }

    void reset()
    {
        initialize(std::nullopt);
    }

private:
    const int m_subperiodCount;
    const std::chrono::microseconds m_subperiod;
    int m_front = 0;
    int m_back = m_subperiodCount - 1;
    std::vector<Value> m_sumPerSubperiod;
    Value m_currentSum = Value();
    std::optional<std::chrono::steady_clock::time_point> m_currentPeriodStart;

    void initialize(std::optional<std::chrono::steady_clock::time_point> currentPeriodStart)
    {
        m_front = 0;
        m_back = m_subperiodCount - 1;
        std::fill(m_sumPerSubperiod.begin(), m_sumPerSubperiod.end(), Value{});
        m_currentSum = Value{};
        m_currentPeriodStart = currentPeriodStart;
    }

    void closeExpiredPeriods(
        std::chrono::steady_clock::time_point now)
    {
        const auto deadline = now - m_subperiod * m_subperiodCount;

        if (!m_currentPeriodStart || deadline > *m_currentPeriodStart + m_subperiod)
            return initialize(now);

        // TODO: #akolesnikov The following loop can be greatly simplified
        // if add expiration time to m_sumPerSubperiod.

        for (int i = 0; i < m_subperiodCount; ++i)
        {
            const auto lastPeriodStart =
                *m_currentPeriodStart - (m_subperiodCount - 1) * m_subperiod;
            const auto lastPeriodEnd = lastPeriodStart + m_subperiod;
            if (lastPeriodEnd > deadline)
                break;

            m_currentSum -= m_sumPerSubperiod[m_back];
            m_sumPerSubperiod[m_back] = Value{};

            if (--m_back < 0)
                m_back = m_subperiodCount - 1;

            if (--m_front < 0)
                m_front = m_subperiodCount - 1;

            *m_currentPeriodStart += m_subperiod;
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
