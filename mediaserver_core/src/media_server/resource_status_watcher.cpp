#include "resource_status_watcher.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include <common/common_module.h>
#include <core/resource/camera_resource.h>

QnResourceStatusWatcher::QnResourceStatusWatcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(commonModule->resourcePool(), &QnResourcePool::statusChanged, this, &QnResourceStatusWatcher::at_resource_statusChanged);
    connect(this, &QnResourceStatusWatcher::statusChanged, this, &QnResourceStatusWatcher::updateResourceStatusInternal, Qt::QueuedConnection);
}

void QnResourceStatusWatcher::updateResourceStatus(const QnResourcePtr& resource)
{
    emit statusChanged(resource);
}

bool QnResourceStatusWatcher::isSetStatusInProgress(const QnResourcePtr &resource)
{
    return m_setStatusInProgress.contains(resource->getId());
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
