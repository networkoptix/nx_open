/**********************************************************
* 12 feb 2015
* akolesnikov
***********************************************************/

#ifdef USE_OWN_MUTEX

#include "mutex_impl.h"

#include "../common/long_runnable.h"


QnMutexImpl::QnMutexImpl( QMutex::RecursionMode mode, QnMutex* const _mutexPtr )
:
    mutex( mode ),
    threadHoldingMutex( 0 ),
    recursiveLockCount( 0 ),
    mutexPtr( _mutexPtr ),
    recursive( mode == QMutex::Recursive )
{
#ifdef ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK
    MutexLockAnalyzer::instance()->mutexCreated( mutexPtr );
#endif
}

QnMutexImpl::~QnMutexImpl()
{
#ifdef ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK
    MutexLockAnalyzer::instance()->beforeMutexDestruction( mutexPtr );
#endif
}

void QnMutexImpl::afterMutexLocked(
    const char* sourceFile,
    int sourceLine,
    size_t lockID )
{
    threadHoldingMutex = QnLongRunnable::currentThreadSystemId();
    ++recursiveLockCount;

#ifdef ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK
    MutexLockKey lockKey(
        sourceFile,
        sourceLine,
        mutexPtr,
        lockID,
        threadHoldingMutex );
    //recursive mutex can be locked multiple times, that's why we need stack
    MutexLockAnalyzer::instance()->afterMutexLocked( lockKey );
    currentLockStack.push( std::move(lockKey) );
#else
    Q_UNUSED( sourceFile );
    Q_UNUSED( sourceLine );
    Q_UNUSED( lockID );
#endif
}

void QnMutexImpl::beforeMutexUnlocked()
{
#ifdef ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK
    MutexLockAnalyzer::instance()->beforeMutexUnlocked( currentLockStack.top() );
    currentLockStack.pop();
#endif

    if( --recursiveLockCount == 0 )
        threadHoldingMutex = 0;
}

#endif  //USE_OWN_MUTEX
