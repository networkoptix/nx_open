#pragma once

#include <functional>
#include <chrono>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

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
     *  @param valGenerationFunc This functor is called from get() and update() to get value, the
     *      call to valGenerationFunc is synchronised by mutex.
     *  @param expirationTime CachedValue will automatically update value on get() or
     *      update() every expirationTime milliseconds. Setting to 0 ms disables expiration.
     *  @note valGenerationFunc is not called here!
     */
    template<class FuncType>
    CachedValue(
        FuncType&& valGenerationFunc,
        std::chrono::milliseconds expirationTime = std::chrono::milliseconds(0))
    :
        m_valGenerationFunc(std::forward<FuncType>(valGenerationFunc)),
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
            m_timer.restart();
        }

        if (!m_value)
            m_value = m_valGenerationFunc();

        return m_value.get();
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
        m_value = m_valGenerationFunc();;
        if (m_expirationTime != std::chrono::milliseconds(0))
            m_timer.restart();
    }

private:
    mutable nx::utils::Mutex m_mutex;

    mutable boost::optional<ValueType> m_value;
    std::function<ValueType()> m_valGenerationFunc;

    mutable ElapsedTimer m_timer;
    const std::chrono::milliseconds m_expirationTime;
};

} // namespace nx::utils

