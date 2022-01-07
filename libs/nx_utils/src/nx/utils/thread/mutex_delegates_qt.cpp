// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mutex_delegates_qt.h"

namespace nx {

MutexQtDelegate::MutexQtDelegate(Mutex::RecursionMode mode)
{
    if (mode == Mutex::NonRecursive)
        m_mutex = std::make_unique<QMutex>();
    else
        m_recursiveMutex = std::make_unique<QRecursiveMutex>();
}

void MutexQtDelegate::lock(const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        m_mutex->lock();
    else
        m_recursiveMutex->lock();
}

void MutexQtDelegate::unlock()
{
    if (m_mutex)
        m_mutex->unlock();
    else
        m_recursiveMutex->unlock();
}

bool MutexQtDelegate::tryLock(const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        return m_mutex->try_lock();
    else
        return m_recursiveMutex->try_lock();
}

bool MutexQtDelegate::isRecursive() const
{
    return (bool) m_recursiveMutex;
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
    const auto delegate = static_cast<MutexQtDelegate*>(mutex);
    if (!delegate->m_mutex)
        return true; //< Recursive mutex causes immediate return.

    return m_delegate.wait(
        delegate->m_mutex.get(),
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

} // namespace nx
