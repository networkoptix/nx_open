/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifdef USE_OWN_MUTEX

#include "mutex.h"

#include "mutex_impl.h"
#include "../common/long_runnable.h"


////////////////////////////////////////////////////////////
//// class QnMutex
////////////////////////////////////////////////////////////

QnMutex::QnMutex( QnMutex::RecursionMode mode )
:
    m_impl( new QnMutexImpl(
        mode == QnMutex::Recursive ? QMutex::Recursive  : QMutex::NonRecursive,
        this ) )
{
}

QnMutex::~QnMutex()
{
}

void QnMutex::lock(
    const char* sourceFile,
    int sourceLine,
    int lockID )
{
    m_impl->mutex.lock();

    m_impl->afterMutexLocked( sourceFile, sourceLine, lockID );
}

void QnMutex::unlock()
{
    m_impl->beforeMutexUnlocked();

    m_impl->mutex.unlock();
}

bool QnMutex::tryLock()
{
    if( m_impl->mutex.tryLock() )
    {
        m_impl->threadHoldingMutex = QnLongRunnable::currentThreadSystemId();
        return true;
    }
    return false;
}


////////////////////////////////////////////////////////////
//// class QnMutexLocker
////////////////////////////////////////////////////////////


void saveCurrentFileLineForThread_abrakadabra(
    const char* sourceFile,
    int sourceLine )
{
    //TODO #ak saving file and line er thread
}

////////////////////////////////////////////////////////////
//// class QnMutexLocker
////////////////////////////////////////////////////////////

QnMutexLockerInternal::QnMutexLockerInternal( QnMutex* const mtx )
:
    m_mtx( mtx ),
    m_sourceFile( nullptr ),
    m_sourceLine( 0 ),
    m_locked( false ),
    m_relockCount( 0 )
{
    //TODO #ak retrieving file/line saved by saveCurrentFileLineForThread_abrakadabra

    m_mtx->lock( m_sourceFile, m_sourceLine, m_relockCount );
    m_locked = true;
}

QnMutexLockerInternal::QnMutexLockerInternal(
    QnMutex* const mtx,
    const char* sourceFile,
    int sourceLine )
:
    m_mtx( mtx ),
    m_sourceFile( sourceFile ),
    m_sourceLine( sourceLine ),
    m_locked( false ),
    m_relockCount( 0 )
{
    m_mtx->lock( sourceFile, sourceLine, m_relockCount );
    m_locked = true;
}

QnMutexLockerInternal::~QnMutexLockerInternal()
{
    if( m_locked )
    {
        m_mtx->unlock();
        m_locked = false;
    }
}

QnMutex* QnMutexLockerInternal::mutex()
{
    return m_mtx;
}

void QnMutexLockerInternal::relock()
{
    Q_ASSERT( !m_locked );
    m_mtx->lock( m_sourceFile, m_sourceLine, ++m_relockCount );
    m_locked = true;
}

void QnMutexLockerInternal::unlock()
{
    Q_ASSERT( m_locked );
    m_mtx->unlock();
    m_locked = false;
}

#endif  //USE_OWN_MUTEX
