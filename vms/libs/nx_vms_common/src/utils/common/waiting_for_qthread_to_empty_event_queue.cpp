// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "waiting_for_qthread_to_empty_event_queue.h"

#include <QtCore/QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

WaitingForQThreadToEmptyEventQueue::WaitingForQThreadToEmptyEventQueue(QThread* thread, int howManyTimesToWait)
:
    m_howManyTimesToWait(howManyTimesToWait),
    m_waitsDone( 0 )
{
    moveToThread(thread);
}

void WaitingForQThreadToEmptyEventQueue::join()
{
    m_waitsDone = 0;
    metaObject()->invokeMethod(this, "doneWaiting", Qt::QueuedConnection);
    if (thread() == QThread::currentThread())
    {
        while (m_waitsDone < m_howManyTimesToWait)
            qApp->processEvents();
    }
    else
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        NX_VERBOSE(this, "Start waiting for counter value %1", m_howManyTimesToWait);
        while (m_waitsDone < m_howManyTimesToWait)
            m_condVar.wait(lk.mutex());
        NX_VERBOSE(this, "Waiting for counter value %1 is finished", m_howManyTimesToWait);
    }
}

void WaitingForQThreadToEmptyEventQueue::doneWaiting()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    ++m_waitsDone;
    NX_VERBOSE(this, "Increase doneWaiting counter to %1", m_waitsDone);
    if(m_waitsDone < m_howManyTimesToWait)
    {
        metaObject()->invokeMethod( this, "doneWaiting", Qt::QueuedConnection);
        return;
    }

    m_condVar.wakeAll();
}
