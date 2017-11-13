#include "remote_archive_worker_pool.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

RemoteArchiveWorkerPool::~RemoteArchiveWorkerPool()
{
    decltype(m_workers) workers;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        workers.swap(m_workers);
    }

    nx::utils::TimerManager::instance()->joinAndDeleteTimer(m_timerId);

    for (auto& entry: workers)
    {
        auto& worker = entry.second;
        worker->pleaseStop();
    }

    for (auto& entry: workers)
    {
        auto& worker = entry.second;
        worker->wait();
    }
}

void RemoteArchiveWorkerPool::start()
{
    QnMutexLocker lock(&m_mutex);
    m_timerId = scheduleTaskGrabbing();
}

void RemoteArchiveWorkerPool::cancelTask(const QnUuid& taskId)
{
    QnMutexLocker lock(&m_mutex);
    const auto itr = m_workers.find(taskId);
    if (itr != m_workers.cend())
        itr->second->pleaseStop();
}

void RemoteArchiveWorkerPool::setMaxTaskCount(int maxTaskCount)
{
    m_maxTaskCount = maxTaskCount;
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

            cleanUpUnsafe();
            processTasksUnsafe();
        },
        std::chrono::milliseconds(5000));
}

void RemoteArchiveWorkerPool::processTasksUnsafe()
{
    if (m_workers.size() >= m_maxTaskCount)
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

    auto lockedMap = lockableTaskMap->lock();
    for (auto itr = lockedMap->begin(); itr != lockedMap->end();)
    {
        auto taskId = itr->first;
        if (m_workers.find(taskId) != m_workers.cend())
        {
            // There is already running task with such Id. Ignore it.
            ++itr;
            continue;
        }

        auto worker = std::make_unique<RemoteArchiveWorker>(itr->second);
        m_workers[itr->first] = std::move(worker);
        m_workers[itr->first]->start();

        itr = lockedMap->erase(itr);

        if (m_workers.size() >= m_maxTaskCount)
            break;
    }

    m_timerId = scheduleTaskGrabbing();
}

void RemoteArchiveWorkerPool::cleanUpUnsafe()
{
    for (auto itr = m_workers.begin(); itr != m_workers.end();)
    {
        if (!itr->second->isRunning())
        {
            itr = m_workers.erase(itr);
            continue;
        }

        ++itr;
    }
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
