/**********************************************************
* 12 feb 2015
* akolesnikov
***********************************************************/

#ifdef _DEBUG

#include "mutex_impl.h"


QnMutexImpl::QnMutexImpl( QMutex::RecursionMode mode )
:
    mutex( mode ),
    threadHoldingMutex( 0 ),
    recursiveLockCount( 0 )
{
}

void QnMutexImpl::afterMutexLocked(
    const char* sourceFile,
    int sourceLine,
    int lockID )
{
    MutexLockKey lockKey(
        sourceFile,
        sourceLine,
        this,
        lockID );
    //recursive mutex can be locked multiple times, that's why we need stack
    MutexLockAnalyzer::instance()->afterMutexLocked( lockKey );
    currentLockStack.push( std::move(lockKey) );
}

void QnMutexImpl::beforeMutexUnlocked()
{
    MutexLockAnalyzer::instance()->beforeMutexUnlocked( currentLockStack.top() );
    currentLockStack.pop();
}

#endif
