// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>
#include <QThread>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>


//!Waits until QThread processes all events already-posted to its event loop
/*!
    Able to repeat wait for multiple times. It is necessary in case if posted events produce new events,
    but user have to specify how many times to repeat wait
*/
class NX_VMS_COMMON_API WaitingForQThreadToEmptyEventQueue:
    public QObject,
    public QnJoinable
{
    Q_OBJECT

public:
    /*!
        Object moves itself to the \a thread
    */
    WaitingForQThreadToEmptyEventQueue( QThread* thread, int howManyTimesToWait = 1 );

    //!Implementation of QnJoinable::join
    /*!
        Blocks till thread's event queue processed posted event
    */
    virtual void join() override;

public slots:
    void doneWaiting();

private:
    const int m_howManyTimesToWait;
    int m_waitsDone;
    nx::Mutex m_mutex;
    nx::WaitCondition m_condVar;
};
