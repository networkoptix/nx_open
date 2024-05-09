// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
// requires std::is_constructible_v<Value, int>
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
        if (valueCount == 0)
            return Value(0);
        else
            return static_cast<Value>(cumulativeValue / valueCount);
    }

    void reset()
    {
        m_cumulativeValuePerPeriod.reset();
        m_valueCountPerPeriod.reset();
    }

private:
    SumPerPeriod<typename detail::LongType<Value>::type> m_cumulativeValuePerPeriod;
    SumPerPeriod<unsigned long long> m_valueCountPerPeriod;
};

} // namespace math
} // namespace utils
} // namespace nx
