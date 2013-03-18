////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "task_queue_watcher.h"

#include <QMutexLocker>


TaskQueueWatcher::TaskQueueWatcher( BlockingQueue<StartApplicationTask>* const taskQueue )
:
    m_taskQueue( taskQueue ),
    m_started( false )
{
}

TaskQueueWatcher::~TaskQueueWatcher()
{
    stop();
}

void TaskQueueWatcher::pleaseStop()
{
    QMutexLocker lk( &m_mutex );
    QnLongRunnable::pleaseStop();
    m_cond.wakeAll();
}

void TaskQueueWatcher::startMonitoring()
{
    QMutexLocker lk( &m_mutex );
    m_started = true;
    m_cond.wakeAll();
}

void TaskQueueWatcher::stopMonitoring()
{
    QMutexLocker lk( &m_mutex );
    m_started = false;
    m_cond.wakeAll();
}

void TaskQueueWatcher::run()
{
    QMutexLocker lk( &m_mutex );
    while( !needToStop() )
    {
        while( !m_started )
        {
            m_cond.wait( lk.mutex() );
            if( needToStop() )
                return;
        }

        while( m_started && m_taskQueue->empty() )
        {
            lk.unlock();
            static const int CHECK_TIMEOUT = 200;
            msleep( CHECK_TIMEOUT );
            lk.relock();
            if( needToStop() )
                return;
        }

        if( m_started )
        {
            emit taskReceived();
            m_started = false;
        }
    }
}
