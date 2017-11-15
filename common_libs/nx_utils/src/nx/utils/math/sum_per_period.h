#pragma once

#include <chrono>
#include <deque>
#include <numeric>

#include <boost/optional.hpp>

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
        const auto now = std::chrono::steady_clock::now();
        if (!m_lastPeriodStart || (*m_lastPeriodStart + m_subperiod < now))
        {
            m_sumPerSubperiod.pop_front();
            m_sumPerSubperiod.push_back(Value());
            m_lastPeriodStart = now;
        }

        m_sumPerSubperiod.back() += value;
    }

    int getSumPerLastPeriod() const
    {
        return std::accumulate(m_sumPerSubperiod.begin(), m_sumPerSubperiod.end(), Value());
    }

private:
    const int m_subPeriodCount;
    const std::chrono::milliseconds m_subperiod;
    std::deque<Value> m_sumPerSubperiod;
    boost::optional<std::chrono::steady_clock::time_point> m_lastPeriodStart;
};

} // namespace math
} // namespace utils
} // namespace nx
