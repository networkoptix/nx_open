#include "remote_archive_worker_pool.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const std::chrono::milliseconds kGrabTasksCycleDuration(5000);

} // namespace

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

void RemoteArchiveWorkerPool::addTask(const RemoteArchiveTaskPtr& task)
{
    QnMutexLocker lock(&m_mutex);

    m_taskQueue.push_back(task);

    processTaskQueueUnsafe();
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
            processTaskQueueUnsafe();
            processTaskMapUnsafe();
            m_timerId = scheduleTaskGrabbing();
        },
        kGrabTasksCycleDuration);
}

void RemoteArchiveWorkerPool::processTaskMapUnsafe()
{
    if (!m_taskMapAccessor)
        return;

    auto lockableTaskMap = m_taskMapAccessor();
    if (!lockableTaskMap)
    {
        NX_ASSERT(false, "No task map provided.");
        return;
    }

    auto lockedMap = lockableTaskMap->lock();
    for (auto itr = lockedMap->begin(); itr != lockedMap->end();)
    {
        if (m_workers.size() >= m_maxTaskCount)
            break;

        if (startWorkerUnsafe(itr->second))
        {
            itr = lockedMap->erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void RemoteArchiveWorkerPool::processTaskQueueUnsafe()
{
    for (auto itr = m_taskQueue.begin(); itr != m_taskQueue.end();)
    {
        if (m_workers.size() >= m_maxTaskCount)
            break;

        if (startWorkerUnsafe(*itr))
        {
            itr = m_taskQueue.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

bool RemoteArchiveWorkerPool::startWorkerUnsafe(const RemoteArchiveTaskPtr& task)
{
    QnUuid taskId = task->id();
    if (m_workers.find(taskId) != m_workers.cend())
        return false; //< There is already running task with such Id. Ignore it.

    auto worker = std::make_unique<RemoteArchiveWorker>(task);
    m_workers[taskId] = std::move(worker);
    m_workers[taskId]->start();
    return true;
}

void RemoteArchiveWorkerPool::cleanUpUnsafe()
{
    for (auto itr = m_workers.begin(); itr != m_workers.end();)
    {
        if (!itr->second->isRunning())
            itr = m_workers.erase(itr);
        else
            ++itr;
    }
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
