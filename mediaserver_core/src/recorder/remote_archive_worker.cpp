#include "remote_archive_worker.h"

namespace nx {
namespace mediaserver_core {
namespace recorder {

RemoteArchiveWorker::RemoteArchiveWorker(RemoteArchiveTaskPtr task):
    m_task(task)
{
}

void RemoteArchiveWorker::run()
{
    if (!m_task)
        return;

    m_task->execute();

    if (m_taskDoneHandler)
        m_taskDoneHandler(m_task->id());
}

void RemoteArchiveWorker::pleaseStop()
{
    if (m_task)
        m_task->cancel();

    base_class::pleaseStop();
}

void RemoteArchiveWorker::setTaskDoneHandler(RemoteArchiveTaskDoneHandler taskDoneHandler)
{
    m_taskDoneHandler = taskDoneHandler;
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
