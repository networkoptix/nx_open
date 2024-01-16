// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <vector>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

namespace nx::utils {

template<typename Value>
class ValueHistory
{
public:
    ValueHistory(
        std::chrono::milliseconds maxAge = std::chrono::hours(1),
        size_t maxSize = 1024 * 1024); //< About 10MB for size_t.

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
    std::chrono::milliseconds fixAge(std::chrono::milliseconds age) const;

private:
    const std::chrono::milliseconds m_maxAge;
    const size_t m_maxSize = 0;
    mutable nx::Mutex m_mutex;
    std::deque<std::pair<Value, std::chrono::steady_clock::time_point>> m_values;
};

// -----------------------------------------------------------------------------------------------

template<typename Value>
ValueHistory<Value>::ValueHistory(std::chrono::milliseconds maxAge, size_t maxSize):
    m_maxAge(maxAge),
    m_maxSize(maxSize)
{
}

template<typename Value>
void ValueHistory<Value>::update(Value value)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    const auto now = monotonicTime();
    if (m_values.empty() || m_values.back().first != value)
        m_values.emplace_back(std::move(value), now);

    if (m_values.size() > m_maxSize)
        m_values.pop_front();

    const auto deadline = now - fixAge(m_maxAge);
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
    NX_MUTEX_LOCKER locker(&m_mutex);
    if (m_values.empty())
        return;

    auto now = monotonicTime();
    auto bound = m_values.begin();
    const auto deadline = now - fixAge(maxAge);
    if (maxAge > m_maxAge / 2)
    {
        decltype(bound) next;
        while ((next = std::next(bound)) != m_values.end() && next->second < deadline)
            bound = next;
    }
    else // Optimization: search for border from the right end because it's closer.
    {
        bound = std::prev(m_values.end());
        while (bound->second >= deadline && bound != m_values.begin())
            bound = std::prev(bound);
    }

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

template<typename Value>
std::chrono::milliseconds ValueHistory<Value>::fixAge(std::chrono::milliseconds age) const
{
    const double delimiter = ini().valueHistoryAgeDelimiter;
    const std::chrono::milliseconds reduced(std::chrono::milliseconds::rep(age.count() / delimiter));
    return std::max(reduced, std::chrono::milliseconds(1));
}

} // namespace nx::utils
