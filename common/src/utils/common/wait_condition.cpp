/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifdef _DEBUG

#include "wait_condition.h"

#include <QtCore/QWaitCondition>

#include "mutex_impl.h"


class QnWaitConditionImpl
{
public:
    QWaitCondition cond;
};


QnWaitCondition::QnWaitCondition()
:
    m_impl( new QnWaitConditionImpl() )
{
}

QnWaitCondition::~QnWaitCondition()
{
}
    
bool QnWaitCondition::wait( QnMutex* mutex, unsigned long time )
{
    mutex->m_impl->beforeMutexUnlocked();
    const bool res = m_impl->cond.wait( &mutex->m_impl->mutex, time );
    //TODO #ak pass proper parameters to the following call
    mutex->m_impl->afterMutexLocked( nullptr, 0, (int)this );
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

#endif
