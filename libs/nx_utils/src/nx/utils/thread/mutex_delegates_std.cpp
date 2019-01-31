// Clang on OSX does not support std::condition_variable_any.
#if !defined(__APPLE__)

#include "mutex_delegates_std.h"

namespace nx::utils {

MutexStdDelegate::MutexStdDelegate(Mutex::RecursionMode mode)
{
    if (mode == Mutex::NonRecursive)
        m_mutex = std::make_unique<std::mutex>();
    else
        m_recursiveMutex = std::make_unique<std::recursive_mutex>();
}

void MutexStdDelegate::lock(const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        m_mutex->lock();
    else
        m_recursiveMutex->lock();
}

bool MutexStdDelegate::tryLock(const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        return m_mutex->try_lock();
    else
        return m_recursiveMutex->try_lock();
}


void MutexStdDelegate::unlock()
{
    if (m_mutex)
        m_mutex->unlock();
    else
        m_recursiveMutex->unlock();
}

bool MutexStdDelegate::isRecursive() const
{
    return m_recursiveMutex.get();
}

//-------------------------------------------------------------------------------------------------

ReadWriteLockStdDelegate::ReadWriteLockStdDelegate(ReadWriteLock::RecursionMode mode)
{
    if (mode == ReadWriteLock::NonRecursive)
        m_mutex = std::make_unique<std::shared_mutex>();
    else
        m_recursiveMutex = std::make_unique<std::recursive_mutex>();
}

void ReadWriteLockStdDelegate::lockForRead(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        m_mutex->lock_shared();
    else
        m_recursiveMutex->lock();
}

void ReadWriteLockStdDelegate::lockForWrite(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        m_mutex->lock();
    else
        m_recursiveMutex->lock();
}

bool ReadWriteLockStdDelegate::tryLockForRead(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        return m_mutex->try_lock_shared();
    else
        return m_recursiveMutex->try_lock();
}

bool ReadWriteLockStdDelegate::tryLockForWrite(
    const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/)
{
    if (m_mutex)
        return m_mutex->try_lock();
    else
        return m_recursiveMutex->try_lock();
}

void ReadWriteLockStdDelegate::unlock()
{
    if (m_mutex)
        return m_mutex->unlock();
    else
        return m_recursiveMutex->unlock();
}

//-------------------------------------------------------------------------------------------------

template<typename Mutex>
bool waitOn(std::condition_variable_any* condition, Mutex* mutex, std::chrono::milliseconds timeout)
{
    std::unique_lock<Mutex> lock(*mutex, std::adopt_lock);
    bool result = true;

    if (timeout != std::chrono::milliseconds::max())
        condition->wait(lock);
    else
        result = (condition->wait_for(lock, timeout) == std::cv_status::no_timeout);

    lock.release();
    return result;
}

bool WaitConditionStdDelegate::wait(MutexDelegate* mutex, std::chrono::milliseconds timeout)
{
    const auto delegate = static_cast<MutexStdDelegate*>(mutex);
    if (delegate->m_mutex)
        return waitOn(&m_condition, delegate->m_mutex.get(), timeout);
    else
        return waitOn(&m_condition, delegate->m_recursiveMutex.get(), timeout);
}

bool WaitConditionStdDelegate::wait(ReadWriteLockDelegate* mutex, std::chrono::milliseconds timeout)
{
    const auto delegate = static_cast<ReadWriteLockStdDelegate*>(mutex);
    if (delegate->m_mutex)
        return waitOn(&m_condition, delegate->m_mutex.get(), timeout);
    else
        return waitOn(&m_condition, delegate->m_recursiveMutex.get(), timeout);
}

void WaitConditionStdDelegate::wakeAll()
{
    m_condition.notify_all();
}

void WaitConditionStdDelegate::wakeOne()
{
    m_condition.notify_one();
}

} // namespace nx::utils

#endif // !defined(__APPLE__)
