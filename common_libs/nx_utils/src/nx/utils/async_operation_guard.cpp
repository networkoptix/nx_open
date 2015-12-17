#include "async_operation_guard.h"

namespace nx {
namespace utils {

AsyncOperationGuard::AsyncOperationGuard()
    : m_guard(new SharedGuard)
{
}

AsyncOperationGuard::~AsyncOperationGuard()
{
    m_guard->terminate();
}

AsyncOperationGuard::SharedGuard::SharedGuard()
    : m_terminated(false)
{
}

AsyncOperationGuard::SharedGuard::Lock::Lock(SharedGuard* guard)
    : m_guard(guard)
    , m_locked(guard->begin())
{
}

AsyncOperationGuard::SharedGuard::Lock::Lock(Lock&& rhs)
    : m_guard(rhs.m_guard)
    , m_locked(rhs.m_locked)
{
    rhs.m_locked = false;
}

void AsyncOperationGuard::SharedGuard::Lock::unlock()
{
    if (!m_locked)
        return;

    m_locked = true;
    m_guard->end();
}

AsyncOperationGuard::SharedGuard::Lock::operator bool() const
{
    return m_locked;
}

AsyncOperationGuard::SharedGuard::Lock::~Lock()
{
    unlock();
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

const std::shared_ptr<AsyncOperationGuard::SharedGuard>&
    AsyncOperationGuard::sharedGuard()
{
    return m_guard;
}

} // namespace utils
} // namespace nx
