#include "remote_archive_worker_pool.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

RemoteArchiveWorkerPool::~RemoteArchiveWorkerPool()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    nx::utils::TimerManager::instance()->joinAndDeleteTimer(m_timerId);

    for (auto& worker: m_workers)
        worker->pleaseStop();

    for (auto& worker: m_workers)
        worker->wait();
}

void RemoteArchiveWorkerPool::start()
{
    QnMutexLocker lock(&m_mutex);
    m_currentTasks.clear();
    for (auto& worker: m_workers)
    {
        worker = std::make_unique<RemoteArchiveWorker>();
        worker->setTaskDoneHandler(
            [this](const QnUuid& taskId)
            {
                QnMutexLocker lock(&m_mutex);
                m_currentTasks.erase(taskId);
            });
        worker->start();
    }

    m_timerId = scheduleTaskGrabbing();
}

void RemoteArchiveWorkerPool::cancelTask(const QnUuid& taskId)
{
    QnMutexLocker lock(&m_mutex);
    for (auto& worker: m_workers)
    {
        if (worker->taskId() == taskId)
            worker->cancel();
    }
}

void RemoteArchiveWorkerPool::setMaxTaskCount(int maxTask)
{
    m_workers.resize(maxTask);
}

void RemoteArchiveWorkerPool::setTaskMapAccessor(std::function<LockableTaskMap*()> accessor)
{
    m_taskMapAccessor = accessor;
}

nx::utils::TimerId RemoteArchiveWorkerPool::scheduleTaskGrabbing()
{
    return nx::utils::TimerManager::instance()->addTimer(
        [this](nx::utils::TimerId timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (m_terminated)
                return;

            if (timerId != m_timerId)
                return;

            processTasksUnsafe();
        },
        std::chrono::milliseconds(5000));
}

void RemoteArchiveWorkerPool::processTasksUnsafe()
{
    if (m_currentTasks.size() >= m_workers.size())
    {
        m_timerId = scheduleTaskGrabbing(); //< All workers are busy
        return;
    }

    if (!m_taskMapAccessor)
    {
        NX_ASSERT(false, lit("No task map accessor. Pool is useless"));
        return;
    }

    auto lockableTaskMap = m_taskMapAccessor();
    if (!lockableTaskMap)
    {
        NX_ASSERT(false, "No task map provided.");
        return;
    }

    nx::utils::Locker<TaskMap> locker(lockableTaskMap);
    auto& taskMap = lockableTaskMap->value;

    for (auto itr = taskMap.begin(); itr != taskMap.end();)
    {
        auto taskId = itr->first;
        if (m_currentTasks.find(taskId) != m_currentTasks.cend())
        {
            // There is already running task with such Id. Ignore it.
            ++itr;
            continue;
        }

        for (auto& worker : m_workers)
        {
            if (worker->taskId() == QnUuid()) //< Free worker.
            {
                setTaskUnsafe(worker.get(), itr->second);
                itr = taskMap.erase(itr);
                break;
            }
        }

        if (m_currentTasks.size() >= m_workers.size())
            break;
    }

    m_timerId = scheduleTaskGrabbing();
}

void RemoteArchiveWorkerPool::setTaskUnsafe(
    RemoteArchiveWorker* worker,
    RemoteArchiveTaskPtr task)
{
    if (m_terminated)
        return;

    m_currentTasks.insert(task->id());
    worker->setTask(task);
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
