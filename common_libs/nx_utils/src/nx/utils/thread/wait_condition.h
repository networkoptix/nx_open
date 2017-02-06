/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifndef NX_WAIT_CONDITION_H
#define NX_WAIT_CONDITION_H

#ifdef USE_OWN_MUTEX

#include <climits>
#include <memory>

#include "mutex.h"


class QnWaitConditionImpl;

class NX_UTILS_API QnWaitCondition
{
public:
	QnWaitCondition();
    ~QnWaitCondition();
    
    bool wait(QnMutex* mutex, unsigned long time = ULONG_MAX);
    void wakeAll();
    void wakeOne();

private:
    QnWaitConditionImpl* m_impl;

    QnWaitCondition(const QnWaitCondition&);
    QnWaitCondition& operator=(const QnWaitCondition&);
};

#else   //USE_OWN_MUTEX

#include <QtCore/QWaitCondition>

typedef QWaitCondition QnWaitCondition;

#endif   //USE_OWN_MUTEX

#endif //NX_WAIT_CONDITION_H
