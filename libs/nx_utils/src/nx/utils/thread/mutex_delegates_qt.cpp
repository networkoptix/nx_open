#include "mutex_delegates_qt.h"

namespace nx::utils {

MutexQtDelegate::MutexQtDelegate(Mutex::RecursionMode mode):
    m_delegate(mode == Mutex::Recursive ? QMutex::Recursive : QMutex::NonRecursive)
{
}

void MutexQtDelegate::lock(const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    m_delegate.lock();
}

void MutexQtDelegate::unlock()
{
    m_delegate.unlock();
}

bool MutexQtDelegate::tryLock(const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    return m_delegate.tryLock();
}

bool MutexQtDelegate::isRecursive() const
{
    return m_delegate.isRecursive();
}

//-------------------------------------------------------------------------------------------------

ReadWriteLockQtDelegate::ReadWriteLockQtDelegate(ReadWriteLock::RecursionMode mode):
    m_delegate((mode == ReadWriteLock::Recursive)
        ? QReadWriteLock::Recursive
        : QReadWriteLock::NonRecursive)
{
}

void ReadWriteLockQtDelegate::lockForRead(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    m_delegate.lockForRead();
}

void ReadWriteLockQtDelegate::lockForWrite(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    m_delegate.lockForWrite();
}

bool ReadWriteLockQtDelegate::tryLockForRead(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    return m_delegate.tryLockForRead();
}

bool ReadWriteLockQtDelegate::tryLockForWrite(
        const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    return m_delegate.tryLockForWrite();
}

void ReadWriteLockQtDelegate::unlock()
{
    m_delegate.unlock();
}

//-------------------------------------------------------------------------------------------------

bool WaitConditionQtDelegate::wait(MutexDelegate* mutex, std::chrono::milliseconds timeout)
{
    return m_delegate.wait(
        &static_cast<MutexQtDelegate*>(mutex)->m_delegate,
        timeout == std::chrono::milliseconds::max() ? ULONG_MAX : (unsigned long) timeout.count());
}

bool WaitConditionQtDelegate::wait(ReadWriteLockDelegate* mutex, std::chrono::milliseconds timeout)
{
    return m_delegate.wait(
        &static_cast<ReadWriteLockQtDelegate*>(mutex)->m_delegate,
        timeout == std::chrono::milliseconds::max() ? ULONG_MAX : (unsigned long) timeout.count());
}

void WaitConditionQtDelegate::wakeAll()
{
    m_delegate.wakeAll();
}

void WaitConditionQtDelegate::wakeOne()
{
    m_delegate.wakeOne();
}

} // namespace nx::utils
