#include "resource_status_watcher.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include "api/app_server_connection.h"
//#include "core/resource/media_server_resource.h"

QnResourceStatusWatcher::QnResourceStatusWatcher()
{
    connect(qnResPool, &QnResourcePool::statusChanged, this, &QnResourceStatusWatcher::at_resource_statusChanged);
}

bool QnResourceStatusWatcher::isSetStatusInProgress(const QnResourcePtr &resource)
{
    return m_setStatusInProgress.contains(resource->getId());
}

void QnResourceStatusWatcher::at_resource_statusChanged(const QnResourcePtr &resource)
{
    //Q_ASSERT_X(!resource->hasFlags(Qn::foreigner), Q_FUNC_INFO, "Status changed for foreign resource!");
    //if (resource.dynamicCast<QnMediaServerResource>())
    //    return;

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
    QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatus(resource->getId(), resource->getStatus(), this, &QnResourceStatusWatcher::requestFinished2);
}

void QnResourceStatusWatcher::requestFinished2(int /*reqID*/, ec2::ErrorCode errCode, const QUuid& id)
{
    if (errCode != ec2::ErrorCode::ok)
        qCritical() << "Failed to update resource status" << id.toString();

    m_setStatusInProgress.remove(id);
    if (m_awaitingSetStatus.contains(id)) {
        m_awaitingSetStatus.remove(id);
        updateResourceStatusAsync(qnResPool->getResourceById(id));
    }
}
