#pragma once

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QThreadPool>

#include <set>
#include <map>
#include <vector>
#include <atomic>

#include <nx/mediaserver/server_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <recorder/remote_archive_worker_pool.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/resource_media_layout.h>
#include <server/server_globals.h>
#include <nx/utils/uuid.h>
#include <nx/utils/lockable.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

class RemoteArchiveSynchronizer:
    public QObject,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT
    using TaskMap = nx::utils::Lockable<std::map<QnUuid, RemoteArchiveTaskPtr>>;
public:
    RemoteArchiveSynchronizer(QnMediaServerModule* serverModule);
    virtual ~RemoteArchiveSynchronizer();

public slots:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resourceStateChanged(const QnResourcePtr& resource);

private:
    int maxSynchronizationThreads() const;
    void handleResourceTaskUnsafe(
        const QnSecurityCamResourcePtr& resoource,
        bool archiveCanBeSynchronized);

    std::shared_ptr<AbstractRemoteArchiveSynchronizationTask> makeTask(
        const QnSecurityCamResourcePtr& resource);

private:
    mutable QnMutex m_mutex;
    std::unique_ptr<TaskMap> m_tasks;
    std::map<QnUuid, bool> m_previousStates;
    std::unique_ptr<RemoteArchiveWorkerPool> m_workerPool;
    std::atomic<bool> m_terminated{false};
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
