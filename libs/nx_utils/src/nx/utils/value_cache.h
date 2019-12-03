#pragma once

#include <optional>
#include <chrono>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/elapsed_timer.h>

namespace nx::utils {

/**
 *  Allows caching of the value and automatic cache invalidation. Methods get(), reset() and
 *  update() can be called safely fom different threads.
 */
template<class ValueType>
class CachedValue
{
public:
    /**
     *  @param valueGenerator This functor is called from get() and update() to get value, the
     *      call to valueGenerator is synchronized by mutex.
     *  @param expirationTime CachedValue will automatically update value on get() or
     *      update() every expirationTime milliseconds. Setting to 0 ms disables expiration.
     *  @note valueGenerator is not called here!
     */
    CachedValue(
        MoveOnlyFunc<ValueType()> valueGenerator,
        std::chrono::milliseconds expirationTime = std::chrono::milliseconds::zero())
    :
        m_valueGenerator(std::move(valueGenerator)),
        m_expirationTime(expirationTime)
    {
    }

    ValueType get() const
    {
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (m_value.has_value() && (m_expirationTime == std::chrono::milliseconds::zero()
                    || !m_timer.hasExpired(m_expirationTime)))
            return *m_value;
        }
        ValueType value = getValue();
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_value = std::move(value);
            m_timer.restart();
        }
        return value;
    }

    void reset()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_value.reset();
    }

    void update()
    {
        ValueType value = getValue();
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_value = std::move(value);
            m_timer.restart();
        }
    }

private:
    ValueType getValue() const
    {
        NX_MUTEX_LOCKER lock(&m_mutexGenerator);
        return m_valueGenerator();
    }

    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::Mutex m_mutexGenerator;

    mutable std::optional<ValueType> m_value;
    const MoveOnlyFunc<ValueType()> m_valueGenerator;

    mutable ElapsedTimer m_timer;
    const std::chrono::milliseconds m_expirationTime;
};

} // namespace nx::utils

