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
    std::chrono::steady_clock::time_point time{};
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

template<typename Id, typename Value>
class TreeValueHistory
{
    struct Node;

public:
    class Access
    {
    public:
        explicit Access(Node* node = nullptr);
        ValueHistory<Value>* operator->() const;
        Access operator[](const Id& id) const;

    private:
        Node* node = nullptr;
    };

    Access access(std::optional<Id> id = {});

private:
    struct Node
    {
        nx::utils::Mutex mutex;
        std::unique_ptr<ValueHistory<Value>> history;
        std::map<Id, Node> subNodes;
    };

private:
    Node m_root;
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


template<typename Id, typename Value>
TreeValueHistory<Id, Value>::Access::Access(Node* node):
    node(node)
{
}

template<typename Id, typename Value>
ValueHistory<Value>* TreeValueHistory<Id, Value>::Access::operator->() const
{
    NX_MUTEX_LOCKER locker(&node->mutex);
    if (!node->history)
        node->history = std::make_unique<ValueHistory<Value>>();
    return node->history.get();
}

template<typename Id, typename Value>
typename TreeValueHistory<Id, Value>::Access TreeValueHistory<Id, Value>::Access::operator[](
    const Id& id) const
{
    NX_MUTEX_LOCKER locker(&node->mutex);
    return Access(&node->subNodes[id]);
}

template<typename Id, typename Value>
typename TreeValueHistory<Id, Value>::Access TreeValueHistory<Id, Value>::access(
    std::optional<Id> id)
{
    const Access root(&m_root);
    return id ? root[*id] : root;
}

} // namespace nx::utils
