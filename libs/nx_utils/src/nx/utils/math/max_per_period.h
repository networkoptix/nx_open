// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include "../data_structures/top_queue.h"
#include "../time.h"

namespace nx::utils::math {

namespace detail {

template<typename Value, template<typename> typename Comp>
class Extreme
{
    using time_point = std::chrono::steady_clock::time_point;

public:
    Extreme(std::chrono::milliseconds period);

    void add(const Value& value);

    void reset();

protected:
    Value getExtremePerLastPeriod() const;

private:
    void removeExpiredValues(const time_point& now);

private:
    struct ValueCompContext
    {
        time_point timePoint;
        Value value;

        bool operator>(const ValueCompContext& other) const
        {
            return value > other.value;
        }

        bool operator<(const ValueCompContext& other) const
        {
            return value < other.value;
        }
    };

    const std::chrono::milliseconds m_period;
    nx::utils::TopQueue<ValueCompContext, Comp<ValueCompContext>> m_values;
};

template<typename Value, template<typename> typename Comp>
Extreme<Value, Comp>::Extreme(std::chrono::milliseconds period):
    m_period(period)
{
}

template<typename Value, template<typename> typename Comp>
void Extreme<Value, Comp>::add(const Value& value)
{
    auto now = monotonicTime();
    removeExpiredValues(now);
    m_values.push({now, value});
}

template<typename Value, template<typename> typename Comp>
void Extreme<Value, Comp>::reset()
{

}

template<typename Value, template<typename> typename Comp>
Value Extreme<Value, Comp>::getExtremePerLastPeriod() const
{
    const_cast<Extreme<Value, Comp>*>(this)->removeExpiredValues(monotonicTime());
    return m_values.empty() ? Value() : m_values.top().value;
}

template<typename Value, template<typename> typename Comp>
void Extreme<Value, Comp>::removeExpiredValues(const time_point& now)
{
    while (!m_values.empty() && m_values.front().timePoint < now - m_period)
        m_values.pop();
}

} // namespace detail

template<typename Value>
class MaxPerPeriod: public detail::Extreme<Value, std::greater>
{
    using base_type = detail::Extreme<Value, std::greater>;
public:
    using base_type::base_type;

    Value getMaxPerLastPeriod() const
    {
        return this->getExtremePerLastPeriod();
    }
};


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


//-------------------------------------------------------------------------------------------------


template<typename Value>
class MinPerPeriod: public detail::Extreme<Value, std::less>
{
    using base_type = detail::Extreme<Value, std::less>;
public:
    using base_type::base_type;

    Value getMinPerLastPeriod() const
    {
        return this->getExtremePerLastPeriod();
    }
};

template<typename Value>
class MinPerMinute: public MaxPerPeriod<Value>
{
    using base_type = MinPerPeriod<Value>;
public:
    MinPerMinute():
        base_type(std::chrono::minutes(1))
    {
    }
};


} // namespace nx::utils::math
