// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "average_per_period.h"
#include "max_per_period.h"
#include "sum_per_period.h"
#include <nx/utils/type_traits.h>

namespace nx::utils::math {

template<typename Value>
struct SummaryStatistics
{
    Value sum{};
    Value min{};
    Value max{};
    Value average{};

    SummaryStatistics()
        :
        sum(Value{}),
        min(std::numeric_limits<Value>::max()),
        max(std::numeric_limits<Value>::min()),
        average(Value{})
    {
        if constexpr (nx::utils::IsDurationV<Value>)
        {
            min = Value::max();
            max = Value::min();
        }
    }
};

template<typename Value>
class SummaryStatisticsPerPeriod
{
public:
    SummaryStatisticsPerPeriod(std::chrono::milliseconds period)
        :
        m_sum(period),
        m_min(period),
        m_max(period),
        m_average(period)
    {}

    SummaryStatistics<Value> summaryStatisticsPerLastPeriod() const
    {
        SummaryStatistics<Value> result;

        result.sum = m_sum.getSumPerLastPeriod();
        result.min = m_min.getMinPerLastPeriod();
        result.max = m_max.getMaxPerLastPeriod();
        result.average = m_average.getAveragePerLastPeriod();

        return result;
    }

    void add(Value value)
    {
        m_sum.add(value);
        m_min.add(value);
        m_max.add(value);
        m_average.add(value);
    }

    void reset()
    {
        m_sum.reset();
        m_min.reset();
        m_max.reset();
        m_average.reset();
    }

private:
    // TODO: There is enough information to determine all of these with one data structure, the
    // same as used in SumPerPeriod
    SumPerPeriod<Value> m_sum;
    MinPerPeriod<Value> m_min;
    MaxPerPeriod<Value> m_max;
    AveragePerPeriod<Value> m_average;
};

} // namespace nx::utils::math
