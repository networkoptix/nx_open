#include "mutex_delegates_debug.h"

#include <nx/utils/thread/thread_util.h>

namespace nx::utils {

MutexDebugDelegate::MutexDebugDelegate(Mutex::RecursionMode mode, bool isAnalyzerInUse):
    m_mutex(mode == Mutex::Recursive ? QMutex::Recursive : QMutex::NonRecursive),
    m_isAnalyzerInUse(isAnalyzerInUse)
{
    if (m_isAnalyzerInUse)
        MutexLockAnalyzer::instance()->mutexCreated(this);
}

MutexDebugDelegate::~MutexDebugDelegate()
{
    NX_ASSERT(currentLockStack.empty());
    if (m_isAnalyzerInUse)
        MutexLockAnalyzer::instance()->beforeMutexDestruction(this);
}

void MutexDebugDelegate::lock(const char* sourceFile, int sourceLine, int lockId)
{
    m_mutex.lock();
    afterLock(sourceFile, sourceLine, lockId);
}

void MutexDebugDelegate::unlock()
{
    beforeUnlock();
    m_mutex.unlock();
}

bool MutexDebugDelegate::tryLock(const char* sourceFile, int sourceLine, int lockId)
{
    const auto result = m_mutex.tryLock();
    if (result)
        afterLock(sourceFile, sourceLine, lockId);

    return result;
}

bool MutexDebugDelegate::isRecursive() const
{
    return m_mutex.isRecursive();
}


void MutexDebugDelegate::afterLock(const char* sourceFile, int sourceLine, int lockId)
{
    threadHoldingMutex = ::currentThreadSystemId();
    ++recursiveLockCount;

    MutexLockKey lockKey(
        sourceFile,
        sourceLine,
        this,
        lockId,
        threadHoldingMutex,
        isRecursive());

    if (m_isAnalyzerInUse)
        MutexLockAnalyzer::instance()->afterMutexLocked(lockKey);

    currentLockStack.push(std::move(lockKey));
}

void MutexDebugDelegate::beforeUnlock()
{
    if (m_isAnalyzerInUse)
        MutexLockAnalyzer::instance()->beforeMutexUnlocked(currentLockStack.top());

    currentLockStack.pop();

    if (--recursiveLockCount == 0)
        threadHoldingMutex = 0;
}

//-------------------------------------------------------------------------------------------------

ReadWriteLockDebugDelegate::ReadWriteLockDebugDelegate(
    ReadWriteLock::RecursionMode mode, bool isAnalyzerInUse)
:
    m_delegate(
        (mode == ReadWriteLock::Recursive)
            ? Mutex::Recursive
            : Mutex::NonRecursive,
        isAnalyzerInUse)
{
}

void ReadWriteLockDebugDelegate::lockForRead(const char* sourceFile, int sourceLine, int lockId)
{
    m_delegate.lock(sourceFile, sourceLine, lockId);
}

void ReadWriteLockDebugDelegate::lockForWrite(const char* sourceFile, int sourceLine, int lockId)
{
    m_delegate.lock(sourceFile, sourceLine, lockId);
}

bool ReadWriteLockDebugDelegate::tryLockForRead(const char* sourceFile, int sourceLine, int lockId)
{
    return m_delegate.tryLock(sourceFile, sourceLine, lockId);
}

bool ReadWriteLockDebugDelegate::tryLockForWrite(const char* sourceFile, int sourceLine, int lockId)
{
    return m_delegate.tryLock(sourceFile, sourceLine, lockId);
}

void ReadWriteLockDebugDelegate::unlock()
{
    return m_delegate.unlock();
}

//-------------------------------------------------------------------------------------------------

bool WaitConditionDebugDelegate::wait(MutexDelegate* mutex, std::chrono::milliseconds timeout)
{
    auto mutexDelegate = static_cast<MutexDebugDelegate*>(mutex);
    mutexDelegate->beforeUnlock();

    const auto result = m_delegate.wait(
        &mutexDelegate->m_mutex,
        (timeout == std::chrono::milliseconds::max())
            ? ULONG_MAX
            : (unsigned long) timeout.count());

    //< Better to save current position than nothing.
    mutexDelegate->afterLock(__FILE__, __LINE__, 0);
    return result;
}

bool WaitConditionDebugDelegate::wait(
    ReadWriteLockDelegate* mutex, std::chrono::milliseconds timeout)
{
    return wait(&static_cast<ReadWriteLockDebugDelegate*>(mutex)->m_delegate, timeout);
}

void WaitConditionDebugDelegate::wakeAll()
{
    m_delegate.wakeAll();
}

void WaitConditionDebugDelegate::wakeOne()
{
    m_delegate.wakeOne();
}

} // namespace nx::utils

