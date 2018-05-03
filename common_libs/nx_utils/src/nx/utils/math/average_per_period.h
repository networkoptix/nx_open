#pragma once

#include "sum_per_period.h"

namespace nx {
namespace utils {
namespace math {

namespace detail {

template<typename T> struct LongType { using type = T; };
template<> struct LongType<int> { using type = long long; };
template<> struct LongType<unsigned int> { using type = unsigned long long; };

} // namespace detail

template<typename Value>
class AveragePerPeriod
{
public:
    AveragePerPeriod(std::chrono::milliseconds period):
        m_cumulativeValuePerPeriod(period),
        m_valueCountPerPeriod(period)
    {
    }

    void add(Value value)
    {
        m_cumulativeValuePerPeriod.add(value);
        m_valueCountPerPeriod.add(1);
    }

    Value getAveragePerLastPeriod() const
    {
        const auto cumulativeValue = m_cumulativeValuePerPeriod.getSumPerLastPeriod();
        const auto valueCount = m_valueCountPerPeriod.getSumPerLastPeriod();
        return static_cast<Value>(valueCount == 0 ? 0 : cumulativeValue / valueCount);
    }

private:
    SumPerPeriod<typename detail::LongType<Value>::type> m_cumulativeValuePerPeriod;
    SumPerPeriod<typename detail::LongType<Value>::type> m_valueCountPerPeriod;
};

} // namespace math
} // namespace utils
} // namespace nx
