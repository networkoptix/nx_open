#include <QThread>

#include "processor.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource_managment/resource_discovery_manager.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(QnId serverId)
    : m_serverId(serverId)
{
    m_appServer = QnAppServerConnectionFactory::createConnection();
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

        if (m_appServer->addCamera(cameraResource, cameras) != 0)
        {
            qCritical() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Reason: " << m_appServer->getLastError();
            continue;
        }

        // cameras contains updated resource with all fields
        QnResourcePool::instance()->addResource(cameras.first());
    }
}

void QnAppserverResourceProcessor::updateResourceStatusAsync(const QnResourcePtr &resource)
{
    int handle = m_appServer->setResourceStatusAsync(resource->getId(), resource->getStatus(), this, SLOT(requestFinished(QnHTTPRawResponse, int)));
    m_handleToResource.insert(handle, resource);
}

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

bool QnAppserverResourceProcessor::isSetStatusInProgress(const QnResourcePtr &resource)
{
    foreach(const QnResourcePtr& res, m_handleToResource.values())
    {
        if (res == resource)
            return true;
    }
    return false;
}

void QnAppserverResourceProcessor::at_resource_statusChanged(const QnResourcePtr &resource)
{
    if (!isSetStatusInProgress(resource))
        updateResourceStatusAsync(resource);
    else
        m_awaitingSetStatus << resource;
}
