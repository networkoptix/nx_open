#pragma once

#include <set>
#include <list>

#include <recorder/remote_archive_worker.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/lockable.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

using TaskMap = std::map<QnUuid, RemoteArchiveTaskPtr>;
using LockableTaskMap = nx::utils::Lockable<TaskMap>;

class RemoteArchiveWorkerPool
{
public:
    virtual ~RemoteArchiveWorkerPool();

    void start();
    void addTask(const RemoteArchiveTaskPtr& task);
    void cancelTask(const QnUuid& taskId);

    void setMaxTaskCount(int maxTaskCount);

    // Should be called before start.
    void setTaskMapAccessor(nx::utils::MoveOnlyFunc<LockableTaskMap*()> taskMapAccessor);

private:
    nx::utils::TimerId scheduleTaskGrabbing();
    void processTaskMapUnsafe();
    void processTaskQueueUnsafe();
    void cleanUpUnsafe();

    bool startWorkerUnsafe(const RemoteArchiveTaskPtr& task);

private:
    mutable QnMutex m_mutex;
    int m_maxTaskCount = 0;
    std::atomic<bool> m_terminated{false};
    nx::utils::TimerId m_timerId = 0;
    std::map<QnUuid, std::unique_ptr<RemoteArchiveWorker>> m_workers;
    std::list<RemoteArchiveTaskPtr> m_taskQueue;
    nx::utils::MoveOnlyFunc<LockableTaskMap*()> m_taskMapAccessor;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
