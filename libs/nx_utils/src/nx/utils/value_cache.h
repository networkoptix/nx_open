// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx::utils {

/**
 *  Allows caching of the value and automatic cache invalidation. Methods get(), reset() and
 *  update() can be called safely from different threads.
 */
template<class ValueType>
class CachedValue
{
public:
    /**
     *  @param valueGenerator This functor is called from get() and update() to get value, it
     *      must be thread-safe.
     *  @note valueGenerator is not called here!
     */
    CachedValue(MoveOnlyFunc<ValueType()> valueGenerator):
        m_valueGenerator(std::move(valueGenerator))
    {
    }

    ValueType get() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_valueVersion == m_version)
            return m_value;
        return updated(&lock);
    }

    void reset()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        ++m_version;
    }

    void update()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        update(&lock);
    }

    ValueType updated() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return updated(&lock);
    }

protected:

    ValueType updated(nx::Locker<nx::Mutex>* lock) const
    {
        update(lock);
        return m_value;
    }

    void update(nx::Locker<nx::Mutex>* lock) const
    {
        int version = ++m_version;

        lock->unlock();
        ValueType value = m_valueGenerator();
        lock->relock();

        if (version < m_valueVersion)
            return; //< Already updated

        m_valueVersion = version;
        m_value = std::move(value);
    }

protected:
    mutable nx::Mutex m_mutex;

    mutable ValueType m_value;
    mutable int64_t m_valueVersion = -1;
    mutable int64_t m_version = 0;
    const MoveOnlyFunc<ValueType()> m_valueGenerator;
};

/**
 *  Allows caching of the value and automatic cache invalidation. Methods get(), reset() and
 *  update() can be called safely from different threads.
 */
template<class ValueType>
class CachedValueWithTimeout: public CachedValue<ValueType>
{
    using base_type = CachedValue<ValueType>;
    using base_type::m_mutex;
    using base_type::m_valueVersion;
    using base_type::m_version;
    using base_type::m_value;
public:

    /**
     *  @param valueGenerator This functor is called from get() and update() to get value, it
     *      must be thread-safe.
     *  @param expirationTime CachedValue will automatically update value on get() or
     *      update() every expirationTime milliseconds. Setting to 0 ms disables expiration.
     *  @note valueGenerator is not called here!
     */
    CachedValueWithTimeout(
        MoveOnlyFunc<ValueType()> valueGenerator,
        std::chrono::milliseconds expirationTime)
        :
        base_type(std::move(valueGenerator)),
        m_expirationTime(expirationTime)
    {
    }

    ValueType get() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_valueVersion == m_version && !m_timer.hasExpired(m_expirationTime))
            return m_value;
        return updated(&lock);
    }

    // Updated(lock) function is not virtual, so this function is required as well.
    ValueType updated() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return updated(&lock);
    }

    // Update(lock) function is not virtual, so this function is required as well.
    void update()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        update(&lock);
    }

    bool isExpired() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_timer.hasExpired(m_expirationTime);
    }

protected:

    // Update(lock) function is not virtual, so this function is required as well.
    ValueType updated(nx::Locker<nx::Mutex>* lock) const
    {
        update(lock);
        return m_value;
    }

    void update(nx::Locker<nx::Mutex>* lock) const
    {
        base_type::update(lock);
        m_timer.restart();
    }

private:
    mutable ElapsedTimer m_timer;
    const std::chrono::milliseconds m_expirationTime;
};

//-------------------------------------------------------------------------------------------------

/**
 * Caches a value and updates its value periodically from a given function.
 * The class is not thread-safe!
 */
template<typename Value, typename StorageType = Value>
class ValueCacheUnSafe
{
public:
    ValueCacheUnSafe(
        nx::utils::MoveOnlyFunc<Value()> generator,
        std::chrono::milliseconds valueUpdatePeriod)
        :
        m_generator(std::move(generator)),
        m_valueUpdatePeriod(valueUpdatePeriod)
    {
    }

    virtual ~ValueCacheUnSafe() = default;

    Value get()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_lastUpdateTime.load() > m_valueUpdatePeriod)
            generateValue();

        return m_value;
    }

protected:
    virtual void generateValue()
    {
        m_value = m_generator();
        m_lastUpdateTime.store(std::chrono::steady_clock::now());
    }

private:
    nx::utils::MoveOnlyFunc<Value()> m_generator;
    const std::chrono::milliseconds m_valueUpdatePeriod;
    std::atomic<std::chrono::steady_clock::time_point> m_lastUpdateTime;
    StorageType m_value = {};
};

//-------------------------------------------------------------------------------------------------

/**
 * Caches a value and updates its value periodically from a given function.
 * Locks internal mutex when value update is needed only.
 * So, this class is suitable for multithreaded environment with a lot of concurrent value access.
 * Value type must be suitable for std::atomic<Value>.
 */
template<typename Value>
class AtomicValueCache:
    public ValueCacheUnSafe<Value, std::atomic<Value>>
{
    using base_type = ValueCacheUnSafe<Value, std::atomic<Value>>;

public:
    using base_type::base_type;

protected:
    virtual void generateValue() override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        base_type::generateValue();
    }

private:
    nx::Mutex m_mutex;
};

} // namespace nx::utils
