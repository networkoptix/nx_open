#include "wearable_archive_synchronizer.h"

#include <nx/utils/std/cpp14.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/synctime.h>

#include "remote_archive_worker_pool.h"


namespace nx {
namespace mediaserver_core {
namespace recorder {

WearableArchiveSynchronizer::WearableArchiveSynchronizer(QnMediaServerModule* serverModule) :
    nx::mediaserver::ServerModuleAware(serverModule),
    m_workerPool(std::make_unique<RemoteArchiveWorkerPool>())
{
    m_workerPool->setMaxTaskCount(maxSynchronizationThreads());
    m_workerPool->start();

    connect(
        commonModule()->resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &WearableArchiveSynchronizer::at_resourceAdded);

    connect(
        commonModule()->resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &WearableArchiveSynchronizer::at_resourceRemoved);
}

WearableArchiveSynchronizer::~WearableArchiveSynchronizer()
{
    QnMutexLocker lock(&m_mutex);
    disconnect(this);
    m_terminated = true;
    m_workerPool.reset();
}

int WearableArchiveSynchronizer::maxSynchronizationThreads() const
{
    auto maxThreads = qnGlobalSettings->maxWearableArchiveSynchronizationThreads();
    if (maxThreads > 0)
        return maxThreads;

    maxThreads = QThread::idealThreadCount() / 2;
    if (maxThreads > 0)
        return maxThreads;

    return 1;
}

void WearableArchiveSynchronizer::at_resourceAdded(const QnResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    connect(
        camera,
        &QnResource::parentIdChanged,
        this,
        &WearableArchiveSynchronizer::at_resource_parentIdChanged);
}

void WearableArchiveSynchronizer::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    disconnect(camera, nullptr, this, nullptr);
    cancelAllTasks(camera);
}

void WearableArchiveSynchronizer::at_resource_parentIdChanged(const QnResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    if (camera->hasFlags(Qn::foreigner))
        cancelAllTasks(camera);
}

int WearableArchiveSynchronizer::progress(const QnUuid& taskId) const
{
    QnMutexLocker lock(&m_mutex);

    auto pos = m_taskInfoById.find(taskId);
    if (pos == m_taskInfoById.end())
        return -1;

    return pos->progress;
}

void WearableArchiveSynchronizer::addTask(const QnResourcePtr& resource, const WearableArchiveTaskPtr& task)
{
    QnUuid taskId = task->id();

    connect(task, &WearableArchiveSynchronizationTask::progressChanged, this,
        [this, taskId](int progress)
        {
            if (m_terminated)
                return;

            QnMutexLocker lock(&m_mutex);

            auto pos = m_taskInfoById.find(taskId);
            if (pos == m_taskInfoById.end())
                return;

            pos->progress = progress;
            pos->updateTime = qnSyncTime->currentMSecsSinceEpoch();
        }
    );

    QnMutexLocker lock(&m_mutex);
    m_taskIdsByResourceId[resource->getId()].push_back(taskId);

    TaskInfo info;
    info.id = taskId;
    info.resourceId = resource->getId();
    info.progress = 0;
    info.updateTime = qnSyncTime->currentMSecsSinceEpoch();
    m_taskInfoById[taskId] = info;
    lock.unlock();

    m_workerPool->addTask(task);
}

void WearableArchiveSynchronizer::cancelAllTasks(const QnResourcePtr& resource)
{
    QList<QnUuid> ids;

    QnMutexLocker lock(&m_mutex);
    ids = m_taskIdsByResourceId.take(resource->getId());
    for (const QnUuid& id : ids)
        m_taskInfoById.remove(id);
    lock.unlock();

    for (const QnUuid& id : ids)
        m_workerPool->cancelTask(id);
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

