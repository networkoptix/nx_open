#ifdef USE_OWN_MUTEX

#include "mutex_impl.h"
#include "thread_util.h"

QnMutexImpl::QnMutexImpl(
    QMutex::RecursionMode mode,
    QnMutex* const _mutexPtr)
    :
    mutex(mode),
    threadHoldingMutex(0),
    recursiveLockCount(0),
    mutexPtr(_mutexPtr),
    recursive(mode == QMutex::Recursive)
{
    MutexLockAnalyzer::instance()->mutexCreated(mutexPtr);
}

QnMutexImpl::~QnMutexImpl()
{
    MutexLockAnalyzer::instance()->beforeMutexDestruction(mutexPtr);
}

void QnMutexImpl::afterMutexLocked(
    const char* sourceFile,
    int sourceLine,
    size_t lockID)
{
    threadHoldingMutex = ::currentThreadSystemId();
    ++recursiveLockCount;

#if defined(_DEBUG) || defined(ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK)
    MutexLockKey lockKey(
        sourceFile,
        sourceLine,
        mutexPtr,
        lockID,
        threadHoldingMutex);

    MutexLockAnalyzer::instance()->afterMutexLocked(lockKey);

    //recursive mutex can be locked multiple times, that's why we need stack
    currentLockStack.push(std::move(lockKey));
#else
    Q_UNUSED(sourceFile);
    Q_UNUSED(sourceLine);
    Q_UNUSED(lockID);
#endif
}

void QnMutexImpl::beforeMutexUnlocked()
{
#if defined(_DEBUG) || defined(ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK)
    MutexLockAnalyzer::instance()->beforeMutexUnlocked(currentLockStack.top());
    currentLockStack.pop();
#endif

    if (--recursiveLockCount == 0)
        threadHoldingMutex = 0;
}

#endif  //USE_OWN_MUTEX
