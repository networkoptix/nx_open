#ifdef USE_OWN_MUTEX

#include "wait_condition.h"

#include <QtCore/QWaitCondition>

#include <nx/utils/time.h>

#include "mutex_impl.h"

class QnWaitConditionImpl
{
public:
    QWaitCondition cond;
};

QnWaitCondition::QnWaitCondition():
    m_impl(new QnWaitConditionImpl())
{
}

QnWaitCondition::~QnWaitCondition()
{
    delete m_impl;
    m_impl = nullptr;
}

bool QnWaitCondition::wait(QnMutex* mutex, unsigned long time)
{
    mutex->m_impl->beforeMutexUnlocked();
    const bool res = m_impl->cond.wait(&mutex->m_impl->mutex, time);
    //TODO #ak pass proper parameters to the following call
    mutex->m_impl->afterMutexLocked(nullptr, 0, (size_t)this);
    return res;
}

void QnWaitCondition::wakeAll()
{
    return m_impl->cond.wakeAll();
}

void QnWaitCondition::wakeOne()
{
    return m_impl->cond.wakeOne();
}

//-------------------------------------------------------------------------------------------------

WaitConditionTimer::WaitConditionTimer(
    QnWaitCondition* waitCondition,
    std::chrono::milliseconds timeout)
    :
    m_waitCondition(waitCondition),
    m_timeout(timeout),
    m_startTime(nx::utils::monotonicTime())
{
}

bool WaitConditionTimer::wait(QnMutex* mutex)
{
    using namespace std::chrono;

    if (m_timeout == std::chrono::milliseconds::max())
    {
        m_waitCondition->wait(mutex);
        return true;
    }

    const auto timePassed = nx::utils::monotonicTime() - m_startTime;
    if (timePassed >= m_timeout)
        return false;

    return m_waitCondition->wait(
        mutex,
        duration_cast<milliseconds>(m_timeout - timePassed).count());
}

#endif
