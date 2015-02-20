/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifndef NX_WAIT_CONDITION_H
#define NX_WAIT_CONDITION_H

#ifdef USE_OWN_MUTEX

#include <memory>

#include "mutex.h"


class QnWaitConditionImpl;

class QnWaitCondition
{
public:
	QnWaitCondition();
    ~QnWaitCondition();
    
    bool wait( QnMutex* mutex, unsigned long time = ULONG_MAX );
    void wakeAll();
    void wakeOne();

private:
    std::unique_ptr<QnWaitConditionImpl> m_impl;
};

#else   //USE_OWN_MUTEX

#include <QtCore/QWaitCondition>

typedef QWaitCondition QnWaitCondition;

#endif   //USE_OWN_MUTEX

#endif //NX_WAIT_CONDITION_H
