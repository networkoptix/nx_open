#include "processor.h"

#include <QtCore/QThread>

#include <core/resource/camera_resource.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_discovery_manager.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(QnId serverId)
    : m_serverId(serverId)
{
    connect(qnResPool, SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    

    foreach (QnResourcePtr resource, resources)
    {
        QnVirtualCameraResourcePtr cameraResource = resource.dynamicCast<QnVirtualCameraResource>();

        if (cameraResource.isNull())
            continue;


        cameraResource->setParentId(m_serverId);
    }

    //QnResourcePool::instance()->addResources(resources);

    // we've got two loops to avoid double call of double sending addCamera

    foreach (QnResourcePtr resource, resources)
    {
        QnVirtualCameraResourcePtr cameraResource = resource.dynamicCast<QnVirtualCameraResource>();
        if (cameraResource.isNull())
            continue;

        // previous comment: camera MUST be in the pool already;
        // but now (new version) camera NOT in resource pool!
        resource->setStatus(QnResource::Online, true); // set status in silence mode. Do not send any signals e.t.c

        QnVirtualCameraResourceList cameras;
        QString password = cameraResource->getAuth().password();


        if (cameraResource->isManuallyAdded() && !QnResourceDiscoveryManager::instance()->containManualCamera(cameraResource->getUrl()))
            continue; //race condition. manual camera just deleted
        const ec2::ErrorCode errorCode = QnAppServerConnectionFactory::getConnection2()->getCameraManager()->addCameraSync( cameraResource, &cameras );
        if( errorCode != ec2::ErrorCode::ok ) {
            qCritical() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Reason: " << ec2::toString(errorCode);
            continue;
        }

        if (cameras.isEmpty())
        {
            qCritical() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Unknown error code. Possible old ECS version is used!";
            continue;
        }

        // cameras contains updated resource with all fields
        QnResourcePool::instance()->addResource(cameras.first());
    }
}

void QnAppserverResourceProcessor::updateResourceStatusAsync(const QnResourcePtr &resource)
{
    if (!resource)
        return;

    m_setStatusInProgress.insert(resource->getId());
    QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatus(resource->getId(), resource->getStatus(), this, &QnAppserverResourceProcessor::requestFinished2);
}

void QnAppserverResourceProcessor::requestFinished2(int /*reqID*/, ec2::ErrorCode errCode, const QnId& id)
{
    if (errCode != ec2::ErrorCode::ok)
        qCritical() << "Failed to update resource status" << id.toString();

    m_setStatusInProgress.remove(id);
    if (m_awaitingSetStatus.contains(id)) {
        m_awaitingSetStatus.remove(id);
        updateResourceStatusAsync(qnResPool->getResourceById(id));
    }
}

#ifdef OLD_EC
void QnAppserverResourceProcessor::requestFinished(const QnHTTPRawResponse& response, int handle)
{
    QnResourcePtr resource = m_handleToResource.value(handle);
    if (resource && response.status != 0)
        qCritical() << "Failed to update resource status" << resource->getId();

    m_handleToResource.remove(handle);

    if (m_awaitingSetStatus.contains(resource)) {
        updateResourceStatusAsync(resource);
        m_awaitingSetStatus.remove(resource);
    }
}
#endif

bool QnAppserverResourceProcessor::isSetStatusInProgress(const QnResourcePtr &resource)
{
    return m_setStatusInProgress.contains(resource->getId());
}

void QnAppserverResourceProcessor::at_resource_statusChanged(const QnResourcePtr &resource)
{
    //Q_ASSERT_X(!resource->hasFlags(QnResource::foreigner), Q_FUNC_INFO, "Status changed for foreign resource!");

    if (!isSetStatusInProgress(resource))
        updateResourceStatusAsync(resource);
    else
        m_awaitingSetStatus << resource->getId();
}
