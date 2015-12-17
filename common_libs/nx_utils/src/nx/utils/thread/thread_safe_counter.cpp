/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#include "thread_safe_counter.h"


ThreadSafeCounter::ThreadSafeCounter()
:
    m_counter(0)
{
}

ThreadSafeCounter::ScopedIncrement ThreadSafeCounter::getScopedIncrement()
{
    return ThreadSafeCounter::ScopedIncrement(this);
}

//!Waits for internal counter to reach zero
void ThreadSafeCounter::wait()
{
    QnMutexLocker lk(&m_mutex);
    while (m_counter > 0)
        m_counterReachedZeroCondition.wait(lk.mutex());
}

void ThreadSafeCounter::increment()
{
    QnMutexLocker lk(&m_mutex);
    ++m_counter;
}

void ThreadSafeCounter::decrement()
{
    QnMutexLocker lk(&m_mutex);
    auto val = --m_counter;
    lk.unlock();
    if (val == 0)
        m_counterReachedZeroCondition.wakeAll();
}
