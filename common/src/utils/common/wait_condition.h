/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifndef NX_WAIT_CONDITION_H
#define NX_WAIT_CONDITION_H

#ifdef _DEBUG

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

#else

#include <QtCore/QWaitCondition>

typedef QWaitCondition QnWaitCondition;

#endif

#endif //NX_WAIT_CONDITION_H
