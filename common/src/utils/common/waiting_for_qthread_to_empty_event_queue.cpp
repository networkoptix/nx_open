/**********************************************************
* 27 dec 2014
* a.kolesnikov
***********************************************************/

#include "waiting_for_qthread_to_empty_event_queue.h"


WaitingForQThreadToEmptyEventQueue::WaitingForQThreadToEmptyEventQueue( QThread* thread, int howManyTimesToWait )
:
    m_howManyTimesToWait( howManyTimesToWait ),
    m_waitsDone( 0 )
{
    moveToThread( thread );
    metaObject()->invokeMethod( this, "doneWaiting", Qt::QueuedConnection );
}

void WaitingForQThreadToEmptyEventQueue::join()
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    while( m_waitsDone < m_howManyTimesToWait )
        m_condVar.wait( lk.mutex() );
}

void WaitingForQThreadToEmptyEventQueue::doneWaiting()
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    ++m_waitsDone;
    if( m_waitsDone < m_howManyTimesToWait )
    {
        metaObject()->invokeMethod( this, "doneWaiting", Qt::QueuedConnection );
        return;
    }

    m_condVar.wakeAll();
}
