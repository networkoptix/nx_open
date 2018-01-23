#pragma once

#include <nx/mediaserver/server_module_aware.h>

#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>

#include "wearable_archive_synchronization_task.h"

namespace nx {
namespace mediaserver_core {
namespace recorder {

class RemoteArchiveWorkerPool;

class WearableArchiveSynchronizer:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT
public:
    WearableArchiveSynchronizer(QnMediaServerModule* serverModule);
    virtual ~WearableArchiveSynchronizer();

    void addTask(const QnResourcePtr& resource, const WearableArchiveTaskPtr& task);
    void cancelAllTasks(const QnResourcePtr& resource);

    int progress(const QnUuid& taskId) const;

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resource_parentIdChanged(const QnResourcePtr& resource);

private:
    int maxSynchronizationThreads() const;

private:
    struct TaskInfo
    {
        QnUuid id;
        QnUuid resourceId;
        int progress = 0;
        qint64 updateTime = 0;
    };

    std::atomic<bool> m_terminated = false;

    mutable QnMutex m_mutex;
    QHash<QnUuid, TaskInfo> m_taskInfoById;
    QHash<QnUuid, QList<QnUuid>> m_taskIdsByResourceId;
    std::unique_ptr<RemoteArchiveWorkerPool> m_workerPool;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx


