#pragma once

#include <QtCore/QObject>

#include "thread/mutex.h"
#include "thread/wait_condition.h"

namespace nx {
namespace utils {

class Counter: public QObject
{
    Q_OBJECT;

public:
    class ScopedIncrement
    {
    public:
        ScopedIncrement(Counter* const counter):
            m_counter(counter)
        {
            if (m_counter)
                m_counter->increment();
        }
        ScopedIncrement(ScopedIncrement&& right):
            m_counter(right.m_counter)
        {
            right.m_counter = nullptr;
        }
        /*!
            TODO #ak remove this constructor after move to msvc2015
        */
        ScopedIncrement(const ScopedIncrement& right):
            m_counter(right.m_counter)
        {
            if (m_counter)
                m_counter->increment();
        }
        ~ScopedIncrement()
        {
            if (m_counter)
                m_counter->decrement();
        }

    private:
        Counter* m_counter;
    };

    Counter(int initialCount = 0, QObject *parent = NULL):
        QObject(parent),
        m_count(initialCount)
    {
    }

    ScopedIncrement getScopedIncrement()
    {
        return Counter::ScopedIncrement(this);
    }
    /**
     * Waits for internal counter to reach zero.
     * @note It makes sense to use this method when there can be no calls to Counter::increment.
     *   E.g., we started several async calls and waiting for their completion before destroying process.
     */
    void wait()
    {
        QnMutexLocker lk(&m_mutex);
        while (m_count > 0)
            m_counterReachedZeroCondition.wait(lk.mutex());
    }

signals:
    void reachedZero();

public slots:
    void increment()
    {
        QnMutexLocker locker( &m_mutex );
        m_count++;
    }

    void decrement()
    {
        QnMutexLocker locker( &m_mutex );
        auto val = --m_count;
        locker.unlock();
        if(val == 0)
        {
            emit reachedZero();
            m_counterReachedZeroCondition.wakeAll();
        }
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_counterReachedZeroCondition;
    int m_count;
};

} // namespace utils
} // namespace nx
