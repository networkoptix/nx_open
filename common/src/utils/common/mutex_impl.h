/**********************************************************
* 12 feb 2015
* akolesnikov
***********************************************************/

#ifdef USE_OWN_MUTEX

#ifndef NX_MUTEX_IMPL_H
#define NX_MUTEX_IMPL_H

#include <cstdint>
#include <stack>

#include <QtCore/QMutex>

#include "../thread/mutex_lock_analyzer.h"


class QnMutexImpl
{
public:
    QMutex mutex;
    //!Thread, that have locked mutex is saved here
    std::uintptr_t threadHoldingMutex;
    size_t recursiveLockCount;
    std::stack<MutexLockKey> currentLockStack;

    QnMutexImpl( QMutex::RecursionMode mode );

    void afterMutexLocked(
        const char* sourceFile,
        int sourceLine,
        int lockID );
    void beforeMutexUnlocked();
};

#endif  //NX_MUTEX_IMPL_H

#endif  //USE_OWN_MUTEX
