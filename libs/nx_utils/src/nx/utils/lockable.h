// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <concepts>
#include <functional>

#include <nx/utils/thread/mutex.h>

namespace nx {

template <typename Value>
class ValueLocker
{
public:
    ValueLocker(nx::Mutex* mutex = nullptr, Value* value = nullptr): m_mutex(mutex), m_value(value)
    {
        if (m_mutex)
            m_mutex->lock();
    }

    ValueLocker(ValueLocker&& other) { *this = std::move(other); }
    ~ValueLocker() { unlock(); }

    ValueLocker(const ValueLocker& other) = delete;
    ValueLocker& operator=(const ValueLocker& other) = delete;

    ValueLocker& operator=(ValueLocker&& other)
    {
        m_mutex = other.m_mutex;
        m_value = other.m_value;
        other.m_mutex = nullptr;
        other.m_value = nullptr;
        return *this;
    }

    void unlock()
    {
        if (m_mutex)
        {
            m_mutex->unlock();
            m_mutex = nullptr;
            m_value = nullptr;
        }
    }

    Value* operator->() { return m_value; }
    const Value* operator->() const { return m_value; }
    Value& operator*() { return *m_value; }
    const Value& operator*() const { return *m_value; }

private:
    Mutex* m_mutex;
    Value* m_value;
};

template<typename Value>
class Lockable
{
public:
    using LockedType = ValueLocker<Value>;

    template<typename... Args>
    Lockable(Args&&... args):
        m_value(std::forward<Args>(args)...)
    {
    }

    LockedType lock()
    {
        return ValueLocker<Value>(&m_mutex, &m_value);
    };

    const LockedType lock() const
    {
        return LockedType(&m_mutex, &m_value);
    };

    /**
     * Calls the visitor with the value while holding the lock, so that a caller needing only a
     * value out of it does not have to name a locker:
     *     const auto size = m_items.visit([](auto& items) { return items.size(); });
     *     const auto items = m_items.visit([](const auto& items) { return items; }); //< A copy.
     * The visitor must not return a reference to the value: the lock is released as visit()
     * returns, so such a reference would be unguarded.
     * @return The visitor result.
     */
    template<std::invocable<Value&> Visit>
    decltype(auto) visit(Visit&& visit)
    {
        ValueLocker<Value> locker(&m_mutex, &m_value);
        return std::invoke(std::forward<Visit>(visit), *locker);
    }

    /**
     * The visitor gets const access, just like the value of lock() const. The constraint is the
     * same as of the overload above on purpose: `std::invocable<const Value&>` would instantiate
     * the body of a generic visitor with a const value, which is a hard error rather than an
     * unsatisfied constraint, so it would break every mutating visitor instead of only the ones
     * called on a const Lockable.
     */
    template<std::invocable<Value&> Visit>
    decltype(auto) visit(Visit&& visit) const
    {
        const ValueLocker<Value> locker(&m_mutex, &m_value);
        return std::invoke(std::forward<Visit>(visit), *locker);
    }

private:
    mutable Mutex m_mutex;

    // TODO: #skolesnik Drop `mutable` - it is only needed because ValueLocker takes a `Value*`,
    // so the const overloads cannot pass `&m_value`. A locker over a const value, or no locker
    // at all, removes the need.
    mutable Value m_value;
};

} // namespace nx
