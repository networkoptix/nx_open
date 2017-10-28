#pragma once

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QThreadPool>

#include <set>
#include <map>
#include <vector>
#include <atomic>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/resource_media_layout.h>
#include <server/server_globals.h>
#include <nx/utils/concurrent.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

struct SynchronizationTaskContext
{
    std::shared_ptr<AbstractRemoteArchiveSynchronizationTask> task;
    nx::utils::concurrent::Future<void> result;
};


class RemoteArchiveSynchronizer:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    RemoteArchiveSynchronizer(QObject* parent);
    virtual ~RemoteArchiveSynchronizer();

public slots:
    void at_newResourceAdded(const QnResourcePtr& resource);
    void at_resourceInitializationChanged(const QnResourcePtr& resource);
    void at_resourceParentIdChanged(const QnResourcePtr& resource);

private:
    int maxSynchronizationThreads() const;
    void makeAndRunTaskUnsafe(const QnSecurityCamResourcePtr& resoource);

    void removeTaskFromAwaited(const QnUuid& resource);
    void cancelTaskForResource(const QnUuid& resource);
    void cancelAllTasks();

    void waitForAllTasks();

private:
    mutable QnMutex m_mutex;
    QThreadPool m_threadPool;
    std::set<QnUuid> m_delayedTasks;
    std::map<QnUuid, SynchronizationTaskContext> m_syncTasks;
    std::atomic<bool> m_terminated;
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
