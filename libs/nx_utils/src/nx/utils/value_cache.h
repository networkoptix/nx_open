#pragma once

#include <optional>
#include <chrono>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/elapsed_timer.h>

namespace nx::utils {

/**
 *  Allows caching of the value and automatical cache invalidation. get() and update() may be
 *  called safely fom different threads.
 */
template<class ValueType>
class CachedValue
{
public:
    /**
     *  @param valueGenerator This functor is called from get() and update() to get value, the
     *      call to valueGenerator is synchronised by mutex.
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
        NX_MUTEX_LOCKER lock(&m_mutex);
        bool shouldGenerateValue = !m_value.has_value()
            || (m_expirationTime != std::chrono::milliseconds::zero()
            && m_timer.hasExpired(m_expirationTime));

        if (shouldGenerateValue)
        {
            m_value = m_valueGenerator();
            m_timer.restart();
        }

        return *m_value;
    }

    void reset()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_value.reset();
    }

    void update()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_value = m_valueGenerator();
        m_timer.restart();
    }

private:
    mutable nx::utils::Mutex m_mutex;

    mutable std::optional<ValueType> m_value;
    MoveOnlyFunc<ValueType()> m_valueGenerator;

    mutable ElapsedTimer m_timer;
    const std::chrono::milliseconds m_expirationTime;
};

} // namespace nx::utils

