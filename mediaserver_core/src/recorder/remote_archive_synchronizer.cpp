#include "remote_archive_synchronizer.h"

#include <recorder/remote_archive_synchronization_task.h>
#include <recorder/remote_archive_stream_synchronization_task.h>
#include <nx/mediaserver/event/event_connector.h>
#include <utils/common/util.h>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/dummy_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

using namespace nx::core::resource;

RemoteArchiveSynchronizer::RemoteArchiveSynchronizer(QObject* parent):
    QnCommonModuleAware(parent),
    m_terminated(false)
{
    if (!qnGlobalSettings->isEdgeRecordingEnabled())
        return;

    NX_LOGX(lit("Creating remote archive synchronizer."), cl_logDEBUG1);

    auto threadCount = maxSynchronizationThreads();
    NX_LOGX(
        lit("Setting maximum number of archive synchronization threads to %1")
        .arg(threadCount),
        cl_logDEBUG1);

    m_threadPool.setMaxThreadCount(threadCount);

    connect(
        commonModule()->resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &RemoteArchiveSynchronizer::at_newResourceAdded);
}

RemoteArchiveSynchronizer::~RemoteArchiveSynchronizer()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    NX_LOGX(lit("Remote archive synchronizer destructor has been called."), cl_logDEBUG1);
    cancelAllTasks();
    waitForAllTasks();
}

void RemoteArchiveSynchronizer::at_newResourceAdded(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    auto securityCameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!securityCameraResource)
        return;

    connect(
        securityCameraResource.data(),
        &QnResource::initializedChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceInitializationChanged);

    connect(
        securityCameraResource.data(),
        &QnSecurityCamResource::scheduleDisabledChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceInitializationChanged);

    connect(
        securityCameraResource.data(),
        &QnResource::parentIdChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceParentIdChanged);
}

void RemoteArchiveSynchronizer::at_resourceInitializationChanged(const QnResourcePtr& resource)
{
    if (m_terminated || resource->hasFlags(Qn::foreigner))
        return;

    auto securityCameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!securityCameraResource)
        return;

    auto archiveCanBeSynchronized = securityCameraResource->isInitialized()
        && securityCameraResource->hasCameraCapabilities(Qn::RemoteArchiveCapability)
        && securityCameraResource->isLicenseUsed()
        && !securityCameraResource->isScheduleDisabled();

    if (!archiveCanBeSynchronized)
    {
        NX_LOGX(lm(
            "Archive for resource %1 can not be synchronized. "
            "Resource is initiliazed: %2, "
            "Resource has remote archive capability: %3, "
            "License is used for resource: %4, "
            "Schedule is enabled for resource: %5")
                .arg(securityCameraResource->getName())
                .arg(securityCameraResource->isInitialized())
                .arg(securityCameraResource->hasCameraCapabilities(Qn::RemoteArchiveCapability))
                .arg(securityCameraResource->isLicenseUsed())
                .arg(!securityCameraResource->isScheduleDisabled()),
            cl_logDEBUG1);

        cancelTaskForResource(securityCameraResource->getId());
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);
        makeAndRunTaskUnsafe(securityCameraResource);
    }
}

void RemoteArchiveSynchronizer::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    NX_LOGX(lm("Resource %1 parent ID has changed to %2")
        .arg(resource->getName())
        .arg(resource->getParentId()),
        cl_logDEBUG1);
    cancelTaskForResource(resource->getId());
}

int RemoteArchiveSynchronizer::maxSynchronizationThreads() const
{
    auto maxThreads = qnGlobalSettings->maxRemoteArchiveSynchronizationThreads();
    if (maxThreads > 0)
        return maxThreads;

    maxThreads = QThread::idealThreadCount() / 2;
    if (maxThreads > 0)
        return maxThreads;

    return 1;
}

void RemoteArchiveSynchronizer::makeAndRunTaskUnsafe(const QnSecurityCamResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto id = resource->getId();

    const auto manager = resource->remoteArchiveManager();
    if (!manager)
        return;

    const auto capabilities = manager->capabilities();
    std::shared_ptr<AbstractRemoteArchiveSynchronizationTask> task;

    if (capabilities.testFlag(RemoteArchiveCapability::RandomAccessChunkCapability))
        task = std::make_shared<RemoteArchiveStreamSynchronizationTask>(commonModule());
    else
        task = std::make_shared<RemoteArchiveSynchronizationTask>(commonModule());

    if (!task)
        return;

    task->setResource(resource);
    task->setDoneHandler([this, id]() { removeTaskFromAwaited(id); });

    SynchronizationTaskContext context;
    context.task = task;
    context.result = nx::utils::concurrent::run(
        &m_threadPool,
        [task]() { task->execute(); });

    m_syncTasks[id] = context;
}

void RemoteArchiveSynchronizer::removeTaskFromAwaited(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    m_syncTasks.erase(id);
}

void RemoteArchiveSynchronizer::cancelTaskForResource(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_syncTasks.find(id);
    if (itr == m_syncTasks.end())
        return;

    auto context = itr->second;
    context.task->cancel();
}

void RemoteArchiveSynchronizer::cancelAllTasks()
{
    QnMutexLocker lock(&m_mutex);
    for (auto& entry: m_syncTasks)
    {
        auto context = entry.second;
        context.task->cancel();
    }
}

void RemoteArchiveSynchronizer::waitForAllTasks()
{
    decltype(m_syncTasks) copy;
    {
        QnMutexLocker lock(&m_mutex);
        copy = m_syncTasks;
    }

    for (auto& item: copy)
    {
        auto context = item.second;
        context.result.waitForFinished();
    }
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
