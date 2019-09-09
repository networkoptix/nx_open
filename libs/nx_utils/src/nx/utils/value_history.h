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

    template<typename Action> // void(const Value& value, std::chrono::milliseconds age)
    void forEach(const Action& action) const { return forEach(m_maxAge, action); }

    template<typename Action> // void(const Value& value, std::chrono::milliseconds age)
    void forEach(std::chrono::milliseconds maxAge, const Action& action) const;

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
template<typename Action> // void(const Value& value, std::chrono::milliseconds age)
void ValueHistory<Value>::forEach(std::chrono::milliseconds maxAge, const Action& action) const
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

    for (auto it = bound; it != m_values.end(); ++it)
    {
        const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
        action(it->first, std::min(age, maxAge));
    }
}

} // namespace nx::utils
