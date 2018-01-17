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

RemoteArchiveSynchronizer::RemoteArchiveSynchronizer(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule),
    m_tasks(std::make_unique<RemoteArchiveSynchronizer::TaskMap>()),
    m_workerPool(std::make_unique<RemoteArchiveWorkerPool>())
{
    if (!qnGlobalSettings->isEdgeRecordingEnabled())
        return;

    NX_LOGX(lit("Creating remote archive synchronizer."), cl_logDEBUG1);

    const auto threadCount = maxSynchronizationThreads();
    NX_LOGX(
        lit("Setting maximum number of archive synchronization threads to %1")
        .arg(threadCount),
        cl_logDEBUG1);

    m_workerPool->setMaxTaskCount(threadCount);
    m_workerPool->setTaskMapAccessor([this](){ return m_tasks.get(); });
    m_workerPool->start();

    connect(
        commonModule()->resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &RemoteArchiveSynchronizer::at_resourceAdded);

    connect(
        commonModule()->resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &RemoteArchiveSynchronizer::at_resourceRemoved);
}

RemoteArchiveSynchronizer::~RemoteArchiveSynchronizer()
{
    QnMutexLocker lock(&m_mutex);
    disconnect(this);
    m_terminated = true;
    m_tasks->lock()->clear();
    m_workerPool.reset();
}

void RemoteArchiveSynchronizer::at_resourceAdded(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    const auto rawPtr = camera.data();

    NX_VERBOSE(this, lm("Resource %1 has been added, connecting to its signals")
        .arg(camera->getUserDefinedName()));

    connect(
        rawPtr,
        &QnResource::statusChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceStateChanged);

    connect(
        rawPtr,
        &QnSecurityCamResource::scheduleDisabledChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceStateChanged);

    connect(
        rawPtr,
        &QnResource::parentIdChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceStateChanged);
}

void RemoteArchiveSynchronizer::at_resourceRemoved(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    NX_VERBOSE(this, lm("Resource %1 has been removed, disconnecting from its signals")
        .arg(camera->getUserDefinedName()));

    disconnect(camera.data(), nullptr, this, nullptr);
    handleResourceTaskUnsafe(camera, /*canArchiveBeSynchronized*/false);
}

void RemoteArchiveSynchronizer::at_resourceStateChanged(const QnResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    const auto status = camera->getStatus();
    const auto canArchiveBeSynchronized =
        (status == Qn::Online || status == Qn::Recording)
        && camera->hasCameraCapabilities(Qn::RemoteArchiveCapability)
        && camera->isLicenseUsed()
        && !camera->isScheduleDisabled()
        && !camera->hasFlags(Qn::foreigner);

    if (!canArchiveBeSynchronized)
    {
        NX_DEBUG(this, lm(
            "Archive for resource %1 can not be synchronized. "
            "Resource belongs to this server %2"
            "Resource status: %3, "
            "Resource has remote archive capability: %4, "
            "License is used for resource: %5, "
            "Schedule is enabled for resource: %6")
                .args(
                    camera->getUserDefinedName(),
                    !camera->hasFlags(Qn::foreigner),
                    status,
                    camera->hasCameraCapabilities(Qn::RemoteArchiveCapability),
                    camera->isLicenseUsed(),
                    !camera->isScheduleDisabled()));
    }

    const auto id = camera->getId();
    bool needToHandleChanges = true;
    {
        QnMutexLocker lock(&m_mutex);
        if (m_terminated)
            return;

        if (m_previousStates.find(id) != m_previousStates.cend())
            needToHandleChanges = m_previousStates.at(id) != canArchiveBeSynchronized;

        m_previousStates[id] = canArchiveBeSynchronized;

        if (needToHandleChanges)
            handleResourceTaskUnsafe(camera, canArchiveBeSynchronized);
    }
}

int RemoteArchiveSynchronizer::maxSynchronizationThreads() const
{
    auto maxThreads = qnGlobalSettings->maxRemoteArchiveSynchronizationThreads();
    if (maxThreads > 0)
        return maxThreads;

    // Ideal thread count is divided by 2 to not fully load
    // CPU in case of motion detection analysis.
    maxThreads = QThread::idealThreadCount() / 2;
    if (maxThreads > 0)
        return maxThreads;

    return 1;
}

void RemoteArchiveSynchronizer::handleResourceTaskUnsafe(
    const QnSecurityCamResourcePtr& resource,
    bool archiveCanBeSynchronized)
{
    if (m_terminated)
        return;

    NX_VERBOSE(this, lm("Remote Archive is going to be synchronized (%1) for resource %2")
        .args(archiveCanBeSynchronized, resource->getUserDefinedName()));

    auto id = resource->getId();
    if (!archiveCanBeSynchronized)
    {
        m_workerPool->cancelTask(id);
        m_tasks->lock()->erase(id);
        return;
    }

    auto task = makeTask(resource);
    if (!task)
        return;

    m_tasks->lock()->emplace(id, task);
}

std::shared_ptr<AbstractRemoteArchiveSynchronizationTask> RemoteArchiveSynchronizer::makeTask(
    const QnSecurityCamResourcePtr& resource)
{
    std::shared_ptr<AbstractRemoteArchiveSynchronizationTask> task;
    const auto manager = resource->remoteArchiveManager();
    if (!manager)
        return task;

    const auto capabilities = manager->capabilities();
    if (capabilities.testFlag(RemoteArchiveCapability::RandomAccessChunkCapability))
    {
        task = std::make_shared<RemoteArchiveStreamSynchronizationTask>(
            serverModule(),
            resource);
    }
    else
    {
        task = std::make_shared<RemoteArchiveSynchronizationTask>(
            serverModule(),
            resource);
    }

    return task;
}

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
