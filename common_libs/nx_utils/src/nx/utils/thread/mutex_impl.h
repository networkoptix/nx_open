#pragma once

#ifdef USE_OWN_MUTEX

#include <cstdint>
#include <stack>

#include <QtCore/QMutex>

#include "mutex_lock_analyzer.h"

class QnMutex;

class NX_UTILS_API QnMutexImpl
{
public:
    QMutex mutex;
    /** Id of the thread that has the mutex locked is saved here. */
    std::uintptr_t threadHoldingMutex;
    size_t recursiveLockCount;
    std::stack<MutexLockKey> currentLockStack;
    QnMutex* const mutexPtr;
    const bool recursive;

    QnMutexImpl(QMutex::RecursionMode mode, QnMutex* const mutexPtr);
    ~QnMutexImpl();

    void afterMutexLocked(
        const char* sourceFile,
        int sourceLine,
        size_t lockID);

    void beforeMutexUnlocked();
};

#endif  //USE_OWN_MUTEX
