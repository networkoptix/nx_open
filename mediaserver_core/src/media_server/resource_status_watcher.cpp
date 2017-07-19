#include "resource_status_watcher.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include <common/common_module.h>
#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <core/resource/camera_resource.h>

namespace {

static constexpr int kServerCheckStatusTimeoutMs = 1000;

} // namespace

QnResourceStatusWatcher::QnResourceStatusWatcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(commonModule->resourcePool(), &QnResourcePool::statusChanged, this, &QnResourceStatusWatcher::at_resource_statusChanged);
    connect(this, &QnResourceStatusWatcher::statusChanged, this, &QnResourceStatusWatcher::updateResourceStatusInternal, Qt::QueuedConnection);
    connect(&m_checkStatusTimer, &QTimer::timeout, this, &QnResourceStatusWatcher::at_timer);
}

QnResourceStatusWatcher::~QnResourceStatusWatcher()
{
    m_checkStatusTimer.stop();
}

void QnResourceStatusWatcher::updateResourceStatus(const QnResourcePtr& resource)
{
    emit statusChanged(resource);
}

bool QnResourceStatusWatcher::isSetStatusInProgress(const QnResourcePtr &resource)
{
    return m_setStatusInProgress.contains(resource->getId());
}

void QnResourceStatusWatcher::addResourcesImmediatly(const QnVirtualCameraResourceList& cameras) const
{
    auto resPool = commonModule()->resourcePool();
    QSet<QString> foreignCameras;
    for (const auto camera: cameras)
        foreignCameras << camera->getPhysicalId();
    QSet<QString> discoveredCameras = commonModule()->resourceDiscoveryManager()->lastDiscoveredIds();
    auto camerasToAddSet = discoveredCameras.intersect(foreignCameras);
    QnResourceList camerasToAdd;
    for (const auto& cameraPhysicalId: camerasToAddSet)
    {
        if (auto res = resPool->getResourceByUniqueId(cameraPhysicalId))
            camerasToAdd << res;
    }
    commonModule()->resourceDiscoveryManager()->addResourcesImmediatly(camerasToAdd);
}

void QnResourceStatusWatcher::at_timer()
{
    auto resPool = commonModule()->resourcePool();
    const auto ownServer = resPool->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if (!ownServer)
        return;
    for (auto itr = m_offlineServersToCheck.begin(); itr != m_offlineServersToCheck.end();)
    {
        QnMediaServerResourcePtr mServer = *itr;
        if (mServer->currentStatusTime() > QnMediaServerResource::kMinFailoverTimeoutMs)
        {
            if (ownServer->isRedundancy() && mServer->getStatus() == Qn::Offline)
                addResourcesImmediatly(resPool->getAllCameras(mServer, /*ignoreDesktop*/ true));
            itr = m_offlineServersToCheck.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
    if (m_offlineServersToCheck.isEmpty())
        m_checkStatusTimer.stop();
}

void QnResourceStatusWatcher::at_resource_statusChanged(const QnResourcePtr &resource, Qn::StatusChangeReason reason)
{
    if (reason == Qn::StatusChangeReason::Local)
        updateResourceStatusInternal(resource);

    const QnSecurityCamResourcePtr& cameraRes = resource.dynamicCast<QnSecurityCamResource>();
    if (cameraRes)
    {
        if (cameraRes->getStatus() >= Qn::Online &&
            !cameraRes->hasFlags(Qn::foreigner) &&
            !cameraRes->isInitialized())
        {
            cameraRes->initAsync(false /*optional*/);
        }
        return;
    }

    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        if (server->getStatus() == Qn::Offline)
        {
            m_offlineServersToCheck << server;
            if (!m_checkStatusTimer.isActive())
                m_checkStatusTimer.start(kServerCheckStatusTimeoutMs);
        }
    }
}

void QnResourceStatusWatcher::updateResourceStatusInternal(const QnResourcePtr& resource)
{
    if (!isSetStatusInProgress(resource))
        updateResourceStatusAsync(resource);
    else
        m_awaitingSetStatus << resource->getId();
}

void QnResourceStatusWatcher::updateResourceStatusAsync(const QnResourcePtr &resource)
{
    if (!resource)
        return;

    m_setStatusInProgress.insert(resource->getId());
    auto connection = commonModule()->ec2Connection();
    auto manager = connection->getResourceManager(Qn::kSystemAccess);
    manager->setResourceStatus(
        resource->getId(),
        resource->getStatus(),
        this,
        &QnResourceStatusWatcher::requestFinished2);
}

void QnResourceStatusWatcher::requestFinished2(int /*reqID*/, ec2::ErrorCode errCode, const QnUuid& id)
{
    if (errCode != ec2::ErrorCode::ok)
        qCritical() << "Failed to update resource status" << id.toString();

    m_setStatusInProgress.remove(id);
    if (m_awaitingSetStatus.contains(id)) {
        m_awaitingSetStatus.remove(id);
        updateResourceStatusAsync(resourcePool()->getResourceById(id));
    }
}
