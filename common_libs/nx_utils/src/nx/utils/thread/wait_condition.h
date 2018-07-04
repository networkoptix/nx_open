#pragma once

#include "mutex.h"

#include <chrono>

#ifdef USE_OWN_MUTEX

#include <climits>
#include <memory>

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

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API WaitConditionTimer
{
public:
    WaitConditionTimer(
        QnWaitCondition* waitCondition,
        std::chrono::milliseconds timeout);

    /**
     * @return false if timeout has passed since object instantiation.
     */
    bool wait(QnMutex* mutex);

private:
    QnWaitCondition* m_waitCondition;
    const std::chrono::milliseconds m_timeout;
    const std::chrono::steady_clock::time_point m_startTime;
};
