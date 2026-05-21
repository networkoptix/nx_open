// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex_locker.h>
#include <nx/utils/thread/wait_condition.h>

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

private:

    ValueType updated(nx::Locker<nx::Mutex>* lock) const
    {
        update(lock);
        return m_value;
    }

    void update(nx::Locker<nx::Mutex>* /*lock*/) const
    {
        int version = ++m_version;
        ValueType value = m_valueGenerator();

        m_valueVersion = version;
        m_value = std::move(value);
    }

private:
    mutable nx::Mutex m_mutex;

    mutable ValueType m_value;
    mutable int64_t m_valueVersion = -1;
    mutable std::atomic<int64_t> m_version = 0;
    const MoveOnlyFunc<ValueType()> m_valueGenerator;
};

/**
 *  Caches a value with an expiration timeout. The slow generator runs on at most one thread
 *  at a time; other concurrent callers either return the previously cached value (if any) or
 *  wait for the in-progress update (on first call when no value exists yet). Methods get() and
 *  reset() can be called safely from different threads.
 */
template<class ValueType>
class CachedValueWithTimeout
{
public:
    /**
     *  @param valueGenerator This functor is called from get() and update() to get value, it
     *      must be thread-safe.
     *  @param expirationTime CachedValue will automatically update value on get()
     *      every expirationTime milliseconds. Setting to 0 ms disables expiration.
     *  @note valueGenerator is not called here!
     */
    CachedValueWithTimeout(
        MoveOnlyFunc<ValueType()> valueGenerator,
        std::chrono::milliseconds expirationTime)
        :
        m_valueGenerator(std::move(valueGenerator)),
        m_expirationTime(expirationTime)
    {
    }

    ValueType get() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        while (true)
        {
            if (m_value.has_value() && !m_timer.hasExpired(m_expirationTime))
                return *m_value;

            if (m_updateInProgress)
            {
                if (m_value.has_value())
                    return *m_value;
                m_updateDone.wait(lock.mutex());
                continue;
            }

            m_updateInProgress = true;
            auto guard = nx::utils::makeScopeGuard(
                [this]()
                {
                    m_updateInProgress = false;
                    m_updateDone.wakeAll();
                });

            std::optional<ValueType> value;
            {
                nx::Unlocker<nx::Mutex> unlocker(&lock);
                value.emplace(m_valueGenerator());
            }

            if (m_resetPending)
            {
                m_resetPending = false;
                continue;
            }

            m_value = std::move(*value);
            m_timer.restart();
            return *m_value;
        }
    }

    void reset() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_value.reset();
        if (m_updateInProgress)
            m_resetPending = true;
    }

    bool isExpired() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_timer.hasExpired(m_expirationTime);
    }

private:
    mutable nx::Mutex m_mutex;
    mutable std::optional<ValueType> m_value;
    const MoveOnlyFunc<ValueType()> m_valueGenerator;
    mutable ElapsedTimer m_timer;
    const std::chrono::milliseconds m_expirationTime;
    mutable bool m_updateInProgress = false;
    mutable bool m_resetPending = false;
    mutable nx::WaitCondition m_updateDone;
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
        nx::MoveOnlyFunc<Value()> generator,
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
    nx::MoveOnlyFunc<Value()> m_generator;
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
