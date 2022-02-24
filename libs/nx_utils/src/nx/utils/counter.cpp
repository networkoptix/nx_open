// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "counter.h"

#include <chrono>

namespace nx {
namespace utils {

//-------------------------------------------------------------------------------------------------
// Counter::ScopedIncrement

Counter::ScopedIncrement::ScopedIncrement(Counter* const counter):
    m_counter(counter)
{
    if (m_counter)
        m_counter->increment();
}

Counter::ScopedIncrement::ScopedIncrement(ScopedIncrement&& right):
    m_counter(right.m_counter)
{
    right.m_counter = nullptr;
}

Counter::ScopedIncrement::ScopedIncrement(const ScopedIncrement& right):
    m_counter(right.m_counter)
{
    if (m_counter)
        m_counter->increment();
}

Counter::ScopedIncrement& Counter::ScopedIncrement::operator=(ScopedIncrement&& right)
{
    if (m_counter)
        m_counter->decrement();

    m_counter = right.m_counter;
    right.m_counter = nullptr;

    return *this;
}

Counter::ScopedIncrement::~ScopedIncrement()
{
    if (m_counter)
        m_counter->decrement();
}

//-------------------------------------------------------------------------------------------------
// Counter

Counter::Counter(int initialCount):
    m_count(initialCount)
{
}

Counter::ScopedIncrement Counter::getScopedIncrement()
{
    return Counter::ScopedIncrement(this);
}

void Counter::wait()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    while (m_count > 0)
        m_counterReachedZeroCondition.wait(lk.mutex());
}

bool Counter::waitFor(std::chrono::milliseconds timeout)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_counterReachedZeroCondition.waitFor(
        lk.mutex(),
        timeout,
        [this]() { return m_count <= 0; });
}

int Counter::increment()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return ++m_count;
}

int Counter::decrement()
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    const auto val = --m_count;
    if (val == 0)
        m_counterReachedZeroCondition.wakeAll();

    return val;
}

//-------------------------------------------------------------------------------------------------

CounterWithSignal::CounterWithSignal(int initialCount, QObject* parent):
    QObject(parent),
    Counter(initialCount)
{
}

int CounterWithSignal::decrement()
{
    const int value = base_type::decrement();

    if (value == 0)
        emit reachedZero();

    return value;
}

} // namespace utils
} // namespace nx
