/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifdef USE_OWN_MUTEX

#include "mutex.h"

#include "mutex_impl.h"
#include "thread_util.h"


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
    if (!m_impl->currentLockStack.empty())
    {
        int* x = nullptr;
        *x = 1;
    }

    delete m_impl;
    m_impl = nullptr;
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
        m_impl->threadHoldingMutex = ::currentThreadSystemId();
        return true;
    }
    return false;
}


////////////////////////////////////////////////////////////
//// class QnMutexLocker
////////////////////////////////////////////////////////////

QnMutexLockerBase::QnMutexLockerBase(
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

QnMutexLockerBase::~QnMutexLockerBase()
{
    if( m_locked )
    {
        m_mtx->unlock();
        m_locked = false;
    }
}

QnMutexLockerBase::QnMutexLockerBase(QnMutexLockerBase&& rhs)
:
    m_mtx(rhs.m_mtx),
    m_sourceFile(rhs.m_sourceFile),
    m_sourceLine(rhs.m_sourceLine),
    m_locked(rhs.m_locked),
    m_relockCount(rhs.m_relockCount)
{
    rhs.m_mtx = nullptr;
    rhs.m_locked = false;
}

QnMutexLockerBase& QnMutexLockerBase::operator=(QnMutexLockerBase&& rhs)
{
    if (this == &rhs)
        return *this;

    if (m_locked)
        unlock();

    m_mtx = rhs.m_mtx;
    m_sourceFile = rhs.m_sourceFile;
    m_sourceLine = rhs.m_sourceLine;
    m_locked = rhs.m_locked;
    m_relockCount = rhs.m_relockCount;

    rhs.m_mtx = nullptr;
    rhs.m_locked = false;

    return *this;
}

QnMutex* QnMutexLockerBase::mutex()
{
    return m_mtx;
}

void QnMutexLockerBase::relock()
{
    Q_ASSERT( !m_locked );
    m_mtx->lock( m_sourceFile, m_sourceLine, ++m_relockCount );
    m_locked = true;
}

void QnMutexLockerBase::unlock()
{
    //assert here to verify that developer knows what he is doing when he tries to unlock already unlocked mutex
    Q_ASSERT( m_locked );
    m_mtx->unlock();
    m_locked = false;
}

bool QnMutexLockerBase::isLocked() const
{
    return m_locked;
}

#endif  //USE_OWN_MUTEX
