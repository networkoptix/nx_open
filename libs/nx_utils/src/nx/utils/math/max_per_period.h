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
    std::chrono::milliseconds elapsedTimeSincePeriodStart(const time_point& timepoint) const;

    void removeExpiredValues();

private:
    const std::chrono::milliseconds m_period;
    std::optional<time_point> m_periodStart;
    nx::utils::TopQueue<std::pair<time_point, Value>> m_values;
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
    if (!m_periodStart)
        m_periodStart = now;

    removeExpiredValues();

    if (elapsedTimeSincePeriodStart(now) > m_period)
        m_periodStart = now;

    m_values.push({now, value});
}

template<typename Value>
Value MaxPerPeriod<Value>::getMaxPerLastPeriod() const
{
    return m_values.empty() ? Value() : m_values.top().second;
}

template<typename Value>
std::chrono::milliseconds MaxPerPeriod<Value>::elapsedTimeSincePeriodStart(
    const MaxPerPeriod<Value>::time_point& timepoint) const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(timepoint - *m_periodStart);
}

template<typename Value>
void MaxPerPeriod<Value>::removeExpiredValues()
{
    while (!m_values.empty() && elapsedTimeSincePeriodStart(m_values.front().first) > m_period)
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