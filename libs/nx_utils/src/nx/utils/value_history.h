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

template<typename Id, typename Value>
class TreeValueHistory
{
    struct Node;

public:
    class Access
    {
    public:
        explicit Access(Node* node = nullptr): m_node(node) {}
        operator bool() const { return m_node; }

    protected:
        Node* m_node = nullptr;
    };

    class Reader: public Access
    {
    public:
        using Access::Access;
        const ValueHistory<Value>* operator->() const;
        Reader operator[](const Id& id) const;
    };

    class Writer: public Access
    {
    public:
        using Access::Access;
        ValueHistory<Value>* operator->() const;
        Writer operator[](const Id& id) const;
    };

    Reader reader(std::optional<Id> id = {});
    Writer writer(std::optional<Id> id = {});

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
        const auto deadline = monotonicTime() - length;
        decltype(bound) next;
        while ((next = std::next(bound)) != m_values.end() && next->time < deadline)
            bound = next;
    }

    std::vector<TimedValue<Value>> values;
    for (auto it = bound; it != m_values.end(); ++it)
        values.push_back(*it);

    return values;
}

template<typename Id, typename Value>
const ValueHistory<Value>* TreeValueHistory<Id, Value>::Reader::operator->() const
{
    static const ValueHistory<Value> kEmptyHistory;
    if (!this->m_node)
        return &kEmptyHistory;

    NX_MUTEX_LOCKER locker(&this->m_node->mutex);
    if (!this->m_node->history)
        return &kEmptyHistory;

    return this->m_node->history.get();
}

template<typename Id, typename Value>
ValueHistory<Value>* TreeValueHistory<Id, Value>::Writer::operator->() const
{
    NX_MUTEX_LOCKER locker(&this->m_node->mutex);
    if (!this->m_node->history)
        this->m_node->history = std::make_unique<ValueHistory<Value>>();

    return this->m_node->history.get();
}

template<typename Id, typename Value>
typename TreeValueHistory<Id, Value>::Reader TreeValueHistory<Id, Value>::Reader::operator[](
    const Id& id) const
{
    NX_MUTEX_LOCKER locker(&this->m_node->mutex);
    if (!this->m_node)
        return Reader(nullptr);

    const auto it = this->m_node->subNodes.find(id);
    return (it != this->m_node->subNodes.end()) ? Reader(&it->second) : Reader(nullptr);
}

template<typename Id, typename Value>
typename TreeValueHistory<Id, Value>::Writer TreeValueHistory<Id, Value>::Writer::operator[](
    const Id& id) const
{
    NX_MUTEX_LOCKER locker(&this->m_node->mutex);
    return Writer(&this->m_node->subNodes[id]);
}

template<typename Id, typename Value>
typename TreeValueHistory<Id, Value>::Reader TreeValueHistory<Id, Value>::reader(
    std::optional<Id> id)
{
    const Reader root(&m_root);
    return id ? root[*id] : root;
}

template<typename Id, typename Value>
typename TreeValueHistory<Id, Value>::Writer TreeValueHistory<Id, Value>::writer(
    std::optional<Id> id)
{
    const Writer root(&m_root);
    return id ? root[*id] : root;
}

} // namespace nx::utils
