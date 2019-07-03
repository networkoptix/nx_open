#include "sync_handler.h"

#include <nx/utils/thread/mutex.h>

namespace ec2::impl {

void SyncHandler::wait()
{
    QnMutexLocker lk(&m_mutex);
    while (!m_done)
        m_cond.wait(lk.mutex());
}

ErrorCode SyncHandler::errorCode() const
{
    QnMutexLocker lk(&m_mutex);
    return m_errorCode;
}

void SyncHandler::done(int /*reqID*/, ErrorCode _errorCode)
{
    QnMutexLocker lk(&m_mutex);
    m_done = true;
    m_errorCode = _errorCode;
    m_cond.wakeAll();
}

} // namespace ec2::impl
