#pragma once

#include <chrono>
#include <deque>
#include <vector>

#include <nx/utils/time.h>
#include <nx/utils/thread/mutex.h>

namespace nx::utils {

template<typename Value>
class ValueHistory
{
public:
    ValueHistory(std::chrono::milliseconds maxAge = std::chrono::hours(1));

    void update(Value value);
    Value current() const;

    struct Border
    {
        enum class Type
        {
            keep, //< Keep closest excluded left value.
            drop, //< Drop excluded left values from results.
            move, //< Move closest excluded left value to the beginning of the interval.
            hardcode, //< Put hardcoded value at the left border if there is(are) any left value(s).
        };

        Type type = move;
        Value value = Value();

        static Border keep() { return {Border::Type::keep, Value()}; }
        static Border drop() { return {Border::Type::drop, Value()}; }
        static Border move() { return {Border::Type::move, Value()}; }
        static Border hardcode(Value value) { return {Border::Type::hardcode, std::move(value)}; }
    };

    template<typename Action> // void(const Value& value, std::chrono::milliseconds duration)
    void forEach(const Action& action, const Border& border = {}) const;

    template<typename Action> // void(const Value& value, std::chrono::milliseconds duration)
    void forEach(
        std::chrono::milliseconds maxAge,
        const Action& action,
        const Border& border = {}) const;

private:
    const std::chrono::milliseconds m_maxAge;
    mutable nx::utils::Mutex m_mutex;
    std::deque<std::pair<Value, std::chrono::steady_clock::time_point>> m_values;
};

// -----------------------------------------------------------------------------------------------

template<typename Value>
ValueHistory<Value>::ValueHistory(std::chrono::milliseconds maxAge):
    m_maxAge(maxAge)
{
}

template<typename Value>
void ValueHistory<Value>::update(Value value)
{
    const auto now = monotonicTime();
    NX_MUTEX_LOCKER locker(&m_mutex);
    if (m_values.empty() || m_values.back().first != value)
        m_values.emplace_back(std::move(value), now);

    const auto deadline = now - m_maxAge;
    while (m_values.size() >= 2 && m_values[1].second < deadline)
        m_values.pop_front();
}

template<typename Value>
Value ValueHistory<Value>::current() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_values.empty() ? Value() : m_values.back().first;
}


template<typename Value>
template<typename Action> // void(const Value& value, std::chrono::milliseconds duration)
void ValueHistory<Value>::forEach(const Action& action, const Border& border) const
{
    return forEach(m_maxAge, action, border);
}

template<typename Value>
template<typename Action> // void(const Value& value, std::chrono::milliseconds duration)
void ValueHistory<Value>::forEach(
    std::chrono::milliseconds maxAge, const Action& action, const Border& border) const
{
    auto now = monotonicTime();
    NX_MUTEX_LOCKER locker(&m_mutex);
    if (m_values.empty())
        return;

    auto bound = m_values.begin();
    const auto deadline = now - maxAge;
    decltype(bound) next;
    while ((next = std::next(bound)) != m_values.end() && next->second < deadline)
        bound = next;

    using namespace std::chrono;
    const auto interval =
        [this, &now](std::chrono::steady_clock::time_point begin, auto it)
        {
            const auto next = std::next(it);
            const auto end = next != m_values.end() ? next->second : now;
            NX_CRITICAL(begin <= end, "begin: %1, end: %2, now: %3", begin, end, now);
            return round<milliseconds>(end - begin);
        };

    if (bound != m_values.end() && bound->second < deadline)
    {
        switch (border.type)
        {
            case Border::Type::keep: action(bound->first, interval(bound->second, bound)); break;
            case Border::Type::move: action(bound->first, interval(deadline, bound)); break;
            case Border::Type::drop: break;
            case Border::Type::hardcode: action(border.value, interval(deadline, bound)); break;
        }
        bound = std::next(bound);
    }

    for (auto it = bound; it != m_values.end(); ++it)
        action(it->first, interval(it->second, it));
}

} // namespace nx::utils
