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

    template<typename Action> // void(const Value& value, std::chrono::milliseconds duration)
    void forEach(const Action& action, bool inOnly = false) const { return forEach(m_maxAge, action, inOnly); }

    template<typename Action> // void(const Value& value, std::chrono::milliseconds duration)
    void forEach(std::chrono::milliseconds maxAge, const Action& action, bool inOnly = false) const;

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
void ValueHistory<Value>::forEach(
    std::chrono::milliseconds maxAge, const Action& action, bool inOnly) const
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

    if (inOnly && bound != m_values.end() && bound->second < deadline)
        bound = std::next(bound);

    for (auto it = bound; it != m_values.end(); ++it)
    {
        const auto begin = std::max(it->second, deadline);
        const auto next = std::next(it);
        const auto end = next != m_values.end() ? next->second : now;
        NX_CRITICAL(begin < end, "begin: %1, end: %2, now: %3", begin, end, now);
        action(it->first, std::chrono::round<std::chrono::milliseconds>(end - begin));
    }
}

} // namespace nx::utils
