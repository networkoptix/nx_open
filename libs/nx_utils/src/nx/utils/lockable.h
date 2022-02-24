// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

template <typename Value>
class ValueLocker
{
public:
    ValueLocker(nx::Mutex* mutex, Value* value):
        m_mutex(mutex),
        m_value(value)
    {
        m_mutex->lock();
    }

    ValueLocker(ValueLocker&& other)
    {
        m_mutex = other.m_mutex;
        m_value = other.m_value;
        other.m_mutex = nullptr;
        other.m_value = nullptr;
    }

    ValueLocker(const ValueLocker& other) = delete;

    ~ValueLocker()
    {
        if (m_mutex)
            m_mutex->unlock();
    }

    ValueLocker& operator=(const ValueLocker& other) = delete;
    Value* operator->() { return m_value; }
    const Value* operator->() const { return m_value; }
    Value& operator*() { return *m_value; }
    const Value& operator*() const { return *m_value; }

private:
    nx::Mutex* m_mutex;
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

private:
    mutable nx::Mutex m_mutex;
    mutable Value m_value;
};


} // namespace utils
} // namespace nx
