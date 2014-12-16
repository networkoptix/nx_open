/**********************************************************
* 16 dec 2014
* a.kolesnikov
***********************************************************/

#include "enable_multi_thread_direct_connection.h"

#include <QtCore/QMutexLocker>


EnableMultiThreadDirectConnection::EnableMultiThreadDirectConnection( QObject* pObject )
:
    m_signalEmitMutex( QMutex::Recursive ),
    m_pObject( pObject )
{
}

EnableMultiThreadDirectConnection::~EnableMultiThreadDirectConnection()
{
}

void EnableMultiThreadDirectConnection::disconnectAndJoin( QObject* receiver )
{
    QObject::disconnect( m_pObject, nullptr, receiver, nullptr );
    QMutexLocker lk( &m_signalEmitMutex );  //waiting for signals to be emitted in other threads to be processed
}
