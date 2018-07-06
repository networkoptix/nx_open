#pragma once

#include <chrono>
#include <deque>
#include <numeric>

#include <boost/optional.hpp>

#include "../time.h"

namespace nx {
namespace utils {
namespace math {

/**
 * Given stream of values returns sum of values per specified period.
 */
template<typename Value>
class SumPerPeriod
{
public:
    SumPerPeriod(std::chrono::milliseconds period):
        m_subPeriodCount(5),
        m_subperiod(period / m_subPeriodCount)
    {
        m_sumPerSubperiod.resize(m_subPeriodCount);
    }

    void add(Value value)
    {
        const auto now = monotonicTime();
        closeExpiredPeriods(now);
        m_sumPerSubperiod.front() += value;
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
                : duration_cast<microseconds>(m_subperiod);
            if (currentPeriodLength >= valuePeriod)
            {
                m_sumPerSubperiod[periodIndex] += value;
                return;
            }

            const auto valuePerCurrentPeriod =
                (Value) (value * ((double) currentPeriodLength.count() / valuePeriod.count()));
            m_sumPerSubperiod[periodIndex] += valuePerCurrentPeriod;
            value -= valuePerCurrentPeriod;
            valuePeriod -= currentPeriodLength;
        }

        // Ignoring value part that corresponds to period older than oldest sub period.
    }

    Value getSumPerLastPeriod() const
    {
        const auto now = monotonicTime();
        const_cast<SumPerPeriod*>(this)->closeExpiredPeriods(now);
        return std::accumulate(m_sumPerSubperiod.begin(), m_sumPerSubperiod.end(), Value());
    }

    float maxError() const
    {
        return 1.0 / m_subPeriodCount;
    }

private:
    const int m_subPeriodCount;
    const std::chrono::milliseconds m_subperiod;
    std::deque<Value> m_sumPerSubperiod;
    boost::optional<std::chrono::steady_clock::time_point> m_currentPeriodStart;

    void closeExpiredPeriods(
        std::chrono::steady_clock::time_point now)
    {
        if (!m_currentPeriodStart)
        {
            m_currentPeriodStart = now;
            return;
        }

        const auto deadline = now - m_subperiod * m_subPeriodCount;
        for (int i = 0; i < m_subPeriodCount; ++i)
        {
            const auto lastPeriodStart =
                *m_currentPeriodStart - (m_subPeriodCount - 1) * m_subperiod;
            if (lastPeriodStart > deadline)
            {
                break;
            }
            else
            {
                m_sumPerSubperiod.pop_back();
                m_sumPerSubperiod.push_front(Value());
                *m_currentPeriodStart += m_subperiod;
            }
        }
    }
};

} // namespace math
} // namespace utils
} // namespace nx
