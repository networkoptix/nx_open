// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include "../time.h"
#include "../data_structures/top_queue.h"

namespace nx::utils::math {

template<typename Value>
class MaxPerPeriod
{
    using time_point = std::chrono::steady_clock::time_point;

public:
    MaxPerPeriod(std::chrono::milliseconds period);

    void add(const Value& value);

    Value getMaxPerLastPeriod() const;

private:
    void removeExpiredValues(const time_point& now);

private:
    struct MaxValueContext
    {
        time_point timePoint;
        Value value;

        bool operator>(const MaxValueContext& other) const
        {
            return value > other.value;
        }
    };

    const std::chrono::milliseconds m_period;
    nx::utils::TopQueue<MaxValueContext, std::greater<MaxValueContext>> m_values;
};

template<typename Value>
MaxPerPeriod<Value>::MaxPerPeriod(std::chrono::milliseconds period):
    m_period(period)
{
}

template<typename Value>
void MaxPerPeriod<Value>::add(const Value& value)
{
    auto now = monotonicTime();
    removeExpiredValues(now);
    m_values.push({now, value});
}

template<typename Value>
Value MaxPerPeriod<Value>::getMaxPerLastPeriod() const
{
    const_cast<MaxPerPeriod<Value>*>(this)->removeExpiredValues(monotonicTime());
    return m_values.empty() ? Value() : m_values.top().value;
}

template<typename Value>
void MaxPerPeriod<Value>::removeExpiredValues(const time_point& now)
{
    while (!m_values.empty() && m_values.front().timePoint < now - m_period)
        m_values.pop();
}

//-------------------------------------------------------------------------------------------------
// MaxPerMinute

template<typename Value>
class MaxPerMinute: public MaxPerPeriod<Value>
{
    using base_type = MaxPerPeriod<Value>;
public:
    MaxPerMinute():
        base_type(std::chrono::minutes(1))
    {
    }
};

} // namespace nx::utils::math
