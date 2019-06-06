#pragma once

#include <chrono>
#include <deque>
#include <vector>

#include <nx/utils/time.h>
#include <nx/utils/thread/mutex.h>

namespace nx::utils {

template<typename Value>
struct TimedValue
{
    Value value{};
    std::chrono::steady_clock::time_point time{0};
};

template<typename Value>
class ValueHistory
{
public:
    ValueHistory(std::chrono::milliseconds length = std::chrono::hours(1));

    void update(Value value);
    Value current() const;

    std::vector<TimedValue<Value>> last(
        std::chrono::milliseconds length = std::chrono::milliseconds::zero()) const;

private:
    const std::chrono::milliseconds m_length;
    mutable nx::utils::Mutex m_mutex;
    std::deque<TimedValue<Value>> m_values;
};

// -----------------------------------------------------------------------------------------------

struct TreeKey
{
    const QString value;
    TreeKey operator[](const QString& subkey) const { return {value + "." + subkey}; }
};

template<typename Value>
class TreeValueHistory
{
public:
    ValueHistory<Value>* value(const TreeKey& key);

private:
    nx::utils::Mutex m_mutex;
    // TODO: This flat map is an abominaion! Should be a proper tree sructure.
    std::map<QString, std::unique_ptr<ValueHistory<Value>>> m_values;
};

template<typename Value>
class TreeValueHistoryInserter
{
public:
    explicit TreeValueHistoryInserter(
        TreeValueHistory<Value>* history,
        std::optional<TreeKey> key = std::nullopt);

    void insert(Value value);
    TreeValueHistoryInserter subValue(const QString& value);

private:
    TreeValueHistory<Value>* m_history;
    std::optional<TreeKey> m_key;
};

// -----------------------------------------------------------------------------------------------

template<typename Value>
ValueHistory<Value>::ValueHistory(std::chrono::milliseconds length):
    m_length(length)
{
}

template<typename Value>
void ValueHistory<Value>::update(Value value)
{
    const auto now = monotonicTime();
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_values.push_back({value, now});

    const auto deadline = now - m_length;
    while (m_values.size() > 2 && m_values[1].time < deadline)
        m_values.pop_front();
}

template<typename Value>
Value ValueHistory<Value>::current() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_values.empty() ? Value() : m_values.back().value;
}

template<typename Value>
std::vector<TimedValue<Value>> ValueHistory<Value>::last(std::chrono::milliseconds length) const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    auto bound = m_values.begin();
    if (length != std::chrono::milliseconds::zero() && bound != m_values.end())
    {
        const auto deadline = monotonicTime() - m_length;
        bound++;
        while (bound != m_values.end() && bound->time < deadline)
            bound++;
    }

    std::vector<TimedValue<Value>> values;
    for (auto it = bound; it != m_values.end(); ++it)
        values.push_back(*it);

    return values;
}

template<typename Value>
ValueHistory<Value>* TreeValueHistory<Value>::value(const TreeKey& key)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    if (const auto it = m_values.find(key.value); it != m_values.end())
        return it->second.get();

    return m_values.emplace(key.value, std::make_unique<ValueHistory<Value>>())
        .first->second.get();
}

template<typename Value>
TreeValueHistoryInserter<Value>::TreeValueHistoryInserter(
    TreeValueHistory<Value>* history, std::optional<TreeKey> key)
:
    m_history(history),
    m_key(std::move(key))
{
}

template<typename Value>
void TreeValueHistoryInserter<Value>::insert(Value value)
{
    NX_CRITICAL(m_key);
    m_history->value(*m_key)->update(std::move(value));
}

template<typename Value>
TreeValueHistoryInserter<Value> TreeValueHistoryInserter<Value>::subValue(const QString& value)
{
    return TreeValueHistoryInserter<Value>(
        m_history, m_key ? (*m_key)[value] : TreeKey{value});
}

} // namespace nx::utils
