#include "remote_archive_worker.h"

namespace nx {
namespace mediaserver_core {
namespace recorder {

RemoteArchiveWorker::~RemoteArchiveWorker()
{
    pleaseStop();
    wait();
}

void RemoteArchiveWorker::run()
{
    while (!needToStop())
    {
        {
            QnMutexLocker lock(&m_mutex);
            while (!m_task && !needToStop())
                m_wait.wait(&m_mutex);

            if (needToStop())
                return;
        }

        m_task->execute();
        const auto id = m_task->id();

        {
            QnMutexLocker lock(&m_mutex);
            m_task.reset();
        }

        if (m_taskDoneHandler)
            m_taskDoneHandler(id);
    }
}

void RemoteArchiveWorker::pleaseStop()
{
    QnMutexLocker lock(&m_mutex);
    m_terminated = true;
    if (m_task)
        m_task->cancel();

    base_class::pleaseStop();
    m_wait.wakeAll();
}

void RemoteArchiveWorker::setTask(const RemoteArchiveTaskPtr& task)
{
    QnMutexLocker lock(&m_mutex);
    if (m_task || m_terminated)
        return;

    m_task = task;
    m_wait.wakeAll();
}

void RemoteArchiveWorker::cancel()
{
    QnMutexLocker lock(&m_mutex);
    if (m_task)
        m_task->cancel();
}

QnUuid RemoteArchiveWorker::taskId() const
{
    QnMutexLocker lock(&m_mutex);
    if (m_task)
        return m_task->id();

    return QnUuid();
}

void RemoteArchiveWorker::setTaskDoneHandler(RemoteArchiveTaskDoneHandler taskDoneHandler)
{
    m_taskDoneHandler = taskDoneHandler;
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
