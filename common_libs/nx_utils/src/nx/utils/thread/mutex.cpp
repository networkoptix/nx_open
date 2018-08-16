#include "mutex.h"

#include "mutex_delegate_factory.h"

namespace nx::utils {

Mutex::Mutex(RecursionMode mode):
    m_delegate(makeMutexDelegate(mode))
{
}

void Mutex::lock(const char* sourceFile, int sourceLine, int lockId)
{
    m_delegate->lock(sourceFile, sourceLine, lockId);
}

void Mutex::unlock()
{
    m_delegate->unlock();
}

bool Mutex::tryLock(const char* sourceFile, int sourceLine, int lockId)
{
    return m_delegate->tryLock(sourceFile, sourceLine, lockId);
}

bool Mutex::isRecursive() const
{
    return m_delegate->isRecursive();
}

MutexLocker::MutexLocker(Mutex* mutex, const char* sourceFile, int sourceLine) :
    Locker<Mutex>(mutex, &Mutex::lock, sourceFile, sourceLine)
{
}

//-------------------------------------------------------------------------------------------------

ReadWriteLock::ReadWriteLock(RecursionMode mode):
    m_delegate(makeReadWriteLockDelegate(mode))
{
}

void ReadWriteLock::lockForRead(const char* sourceFile, int sourceLine, int lockId)
{
    m_delegate->lockForRead(sourceFile, sourceLine, lockId);
}

void ReadWriteLock::lockForWrite(const char* sourceFile, int sourceLine, int lockId)
{
    m_delegate->lockForWrite(sourceFile, sourceLine, lockId);
}

bool ReadWriteLock::tryLockForRead(const char* sourceFile, int sourceLine, int lockId)
{
    return m_delegate->tryLockForRead(sourceFile, sourceLine, lockId);
}

bool ReadWriteLock::tryLockForWrite(const char* sourceFile, int sourceLine, int lockId)
{
    return m_delegate->tryLockForWrite(sourceFile, sourceLine, lockId);
}

void ReadWriteLock::unlock()
{
    m_delegate->unlock();
}

ReadLocker::ReadLocker(ReadWriteLock* mutex, const char* sourceFile, int sourceLine):
    Locker<ReadWriteLock>(mutex, &ReadWriteLock::lockForRead, sourceFile, sourceLine)
{
}

WriteLocker::WriteLocker(ReadWriteLock* mutex, const char* sourceFile, int sourceLine):
    Locker<ReadWriteLock>(mutex, &ReadWriteLock::lockForWrite, sourceFile, sourceLine)
{
}

//-------------------------------------------------------------------------------------------------

WaitCondition::WaitCondition():
    m_delegate(makeWaitConditionDelegate())
{
}

bool WaitCondition::wait(Mutex* mutex, std::chrono::milliseconds timeout)
{
    return m_delegate->wait(mutex->m_delegate.get(), timeout);
}

bool WaitCondition::wait(Mutex* mutex, unsigned long timeout)
{
    return m_delegate->wait(mutex->m_delegate.get(), std::chrono::milliseconds(timeout));
}

bool WaitCondition::wait(ReadWriteLock* mutex, std::chrono::milliseconds timeout)
{
    return m_delegate->wait(mutex->m_delegate.get(), timeout);
}

bool WaitCondition::wait(ReadWriteLock* mutex, unsigned long timeout)
{
    return m_delegate->wait(mutex->m_delegate.get(), std::chrono::milliseconds(timeout));
}

void WaitCondition::wakeAll()
{
    m_delegate->wakeAll();
}

void WaitCondition::wakeOne()
{
    m_delegate->wakeOne();
}

} // namespace nx::utils

