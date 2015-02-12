/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifdef _DEBUG

#include "mutex.h"

#include "mutex_impl.h"


QnMutex::QnMutex( QnMutex::RecursionMode mode )
:
    m_impl( new QnMutexImpl( mode == QnMutex::Recursive
                             ? QMutex::Recursive 
                             : QMutex::NonRecursive ) )
{
}

QnMutex::~QnMutex()
{
}

void QnMutex::lock(
    const char* /*sourceFile*/,
    int /*sourceLine*/,
    int /*lockID*/ )
{
    m_impl->mutex.lock();
}

void QnMutex::unlock()
{
    m_impl->mutex.unlock();
}

bool QnMutex::tryLock()
{
    return m_impl->mutex.tryLock();
}


QnMutexLocker::QnMutexLocker(
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

QnMutexLocker::~QnMutexLocker()
{
    if( m_locked )
    {
        m_mtx->unlock();
        m_locked = false;
    }
}

QnMutex* QnMutexLocker::mutex()
{
    return m_mtx;
}

void QnMutexLocker::relock()
{
    Q_ASSERT( !m_locked );
    m_mtx->lock( m_sourceFile, m_sourceLine, ++m_relockCount );
    m_locked = true;
}

void QnMutexLocker::unlock()
{
    Q_ASSERT( m_locked );
    m_mtx->unlock();
    m_locked = false;
}

#endif
