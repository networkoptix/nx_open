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
    template<class ValueGenerator>
    CachedValue(
        ValueGenerator&& valueGenerator,
        std::chrono::milliseconds expirationTime = std::chrono::milliseconds(0))
    :
        m_valueGenerator(std::forward<ValueGenerator>(valueGenerator)),
        m_expirationTime(expirationTime)
    {
    }

    ValueType get() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_expirationTime != std::chrono::milliseconds(0)
            && m_timer.hasExpired(m_expirationTime))
        {
            resetThreadUnsafe();
        }

        if (!m_value)
        {
            m_value = m_valueGenerator();
            m_timer.restart();
        }

        return *m_value;
    }

    void reset()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        resetThreadUnsafe();
    }

    void resetThreadUnsafe() const
    {
        m_timer.invalidate();
        m_value.reset();
    }

    void update()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_value = m_valueGenerator();;
        if (m_expirationTime != std::chrono::milliseconds(0))
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

