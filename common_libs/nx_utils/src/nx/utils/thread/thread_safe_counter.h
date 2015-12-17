/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_UTILS_THREAD_SAFE_COUNTER_H
#define NX_UTILS_THREAD_SAFE_COUNTER_H

#include "mutex.h"
#include "wait_condition.h"


/*!
    \note Does not contain counter overflow check
*/
class NX_UTILS_API ThreadSafeCounter
{
public:
    class ScopedIncrement
    {
    public:
        ScopedIncrement(ThreadSafeCounter* const counter)
        :
            m_counter(counter)
        {
            if (m_counter)
                m_counter->increment();
        }
        ScopedIncrement(ScopedIncrement&& right)
        :
            m_counter(right.m_counter)
        {
            right.m_counter = nullptr;
        }
        /*!
            TODO #ak remove this constructor after move to msvc2015
        */
        ScopedIncrement(const ScopedIncrement& right)
        :
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
        ThreadSafeCounter* m_counter;
    };

    ThreadSafeCounter();

    ScopedIncrement getScopedIncrement();
    //!Waits for internal counter to reach zero
    /*!
        \note It makes sense to use this method when there can be no calls to \a ThreadSafeCounter::increment.
            E.g., we started several async calls and waiting for their completion before destroying process
    */
    void wait();

private:
    size_t m_counter;
    QnMutex m_mutex;
    QnWaitCondition m_counterReachedZeroCondition;

    void increment();
    void decrement();
};

#endif  //NX_UTILS_THREAD_SAFE_COUNTER_H
