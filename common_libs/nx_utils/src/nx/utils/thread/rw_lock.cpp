#include "rw_lock.h"

namespace nx {
namespace utils {

RwLock::RwLock(int maxNumberOfLocks):
    m_maxNumberOfLocks(maxNumberOfLocks),
    m_readLock(maxNumberOfLocks)
{
}

void RwLock::lock(RwLockType lockType)
{
    if (lockType == RwLockType::read)
    {
        m_readLock.acquire(1);
    }
    else
    {
        m_writeLock.lock();
        m_readLock.acquire(m_maxNumberOfLocks);
    }
}

void RwLock::unlock(RwLockType lockType)
{
    if (lockType == RwLockType::read)
    {
        m_readLock.release(1);
    }
    else
    {
        m_readLock.release(m_maxNumberOfLocks);
        m_writeLock.unlock();
    }
}

RwLocker::RwLocker(RwLock* lock, RwLockType lockType):
    m_lock(lock),
    m_lockType(lockType)
{
    if (!m_lock)
        return;

    m_lock->lock(m_lockType);
}

RwLocker::~RwLocker()
{
    if (!m_lock)
        return;

    m_lock->unlock(m_lockType);
}

} // namespace utils
} // namespace nx
