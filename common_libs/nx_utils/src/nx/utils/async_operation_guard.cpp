#include "async_operation_guard.h"

namespace nx {
namespace utils {

//-------------------------------------------------------------------------------------------------
// AsyncOperationGuard

AsyncOperationGuard::AsyncOperationGuard():
    m_sharedGuard(new SharedGuard)
{
}

AsyncOperationGuard::~AsyncOperationGuard()
{
    m_sharedGuard->terminate();
}

const std::shared_ptr<AsyncOperationGuard::SharedGuard>&
    AsyncOperationGuard::sharedGuard()
{
    return m_sharedGuard;
}

void AsyncOperationGuard::reset()
{
    m_sharedGuard->terminate();
    m_sharedGuard.reset(new SharedGuard);
}

AsyncOperationGuard::SharedGuard* AsyncOperationGuard::operator->() const
{
    return m_sharedGuard.get();
}

//-------------------------------------------------------------------------------------------------
// AsyncOperationGuard::SharedGuard

AsyncOperationGuard::SharedGuard::SharedGuard():
    m_mutex(QnMutex::Recursive),
    m_terminated(false)
{
}

AsyncOperationGuard::SharedGuard::Lock AsyncOperationGuard::SharedGuard::lock()
{
    return Lock(this);
}

void AsyncOperationGuard::SharedGuard::terminate()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
}

bool AsyncOperationGuard::SharedGuard::begin()
{
    m_mutex.lock();
    if (m_terminated)
    {
        m_mutex.unlock();
        return false;
    }

    return true;
}

void AsyncOperationGuard::SharedGuard::end()
{
    m_mutex.unlock();
}

//-------------------------------------------------------------------------------------------------
// AsyncOperationGuard::SharedGuard::Lock

AsyncOperationGuard::SharedGuard::Lock::Lock(SharedGuard* guard):
    m_sharedGuard(guard),
    m_locked(guard->begin())
{
}

AsyncOperationGuard::SharedGuard::Lock::Lock(Lock&& rhs):
    m_sharedGuard(rhs.m_sharedGuard),
    m_locked(rhs.m_locked)
{
    rhs.m_locked = false;
}

void AsyncOperationGuard::SharedGuard::Lock::unlock()
{
    if (!m_locked)
        return;

    m_locked = true;
    m_sharedGuard->end();
}

AsyncOperationGuard::SharedGuard::Lock::operator bool() const
{
    return m_locked;
}

bool AsyncOperationGuard::SharedGuard::Lock::operator!() const
{
    return !m_locked;
}

AsyncOperationGuard::SharedGuard::Lock::~Lock()
{
    unlock();
}

} // namespace utils
} // namespace nx
