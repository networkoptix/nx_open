#pragma once

#include <functional>
#include <chrono>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/thread/mutex.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace utils {

/**
    @a get() and @a update() may be called safely fom different threads
*/
template<class ValueType>
class CachedValue
{
public:
    /**
        @param valGenerationFunc This functor is called from get() and update() with no
            synchronization. So it should be synchronized by itself (if it is needed)
        @param expirationTime CachedValue will automatically update value on @a get() or
            @a update() every @a expirationTime milliseconds. Setting to 0 ms disables expiration.
        @note @a valGenerationFunc is not called here!
    */
    template<class FuncType>
    CachedValue(
        FuncType&& valGenerationFunc,
        QnMutex* const mutex,
        std::chrono::milliseconds expirationTime = std::chrono::milliseconds(0))
    :
        m_valGenerationFunc(std::forward<FuncType>(valGenerationFunc)),
        m_mutex(mutex),
        m_expirationTime(expirationTime)
    {
    }

    ValueType get() const
    {
        QnMutexLocker lk(m_mutex);

        if (m_expirationTime != std::chrono::milliseconds(0)
            && m_timer.hasExpired(m_expirationTime))
        {
            resetThreadUnsafe();
            m_timer.restart();
        }

        if (!m_value)
        {
            lk.unlock();
            const ValueType newVal = m_valGenerationFunc();
            lk.relock();
            if (!m_value)
                m_value = newVal;
        }

        return m_value.get();
    }

    void reset()
    {
        QnMutexLocker lk(m_mutex);
        resetThreadUnsafe();
    }

    void resetThreadUnsafe() const
    {
        m_timer.invalidate();
        m_value.reset();
    }

    void update()
    {
        const ValueType newValue = m_valGenerationFunc();
        QnMutexLocker lk (m_mutex);
        m_value = newValue;
        if (m_expirationTime != std::chrono::milliseconds(0))
            m_timer.restart();
    }

private:
    mutable boost::optional<ValueType> m_value;
    std::function<ValueType()> m_valGenerationFunc;
    QnMutex* const m_mutex;

    mutable ElapsedTimer m_timer;
    const std::chrono::milliseconds m_expirationTime;
};

}
} // namespace nx::utils

