#include <QThread>

#include "processor.h"
#include "core/resourcemanagment/resource_pool.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(QnId serverId)
    : m_serverId(serverId)
{
    m_appServer = QnAppServerConnectionFactory::createConnection();
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    QnVirtualCameraResourceList cameras;

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

        QByteArray errorString;
        if (m_appServer->addCamera(cameraResource, cameras, errorString) != 0)
        {
            qDebug() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Reason: " << errorString;
            continue;
        }

        // cameras contains updated resource with all fields
        QnResourcePool::instance()->addResource(cameras.first());
    }
}

void QnAppserverResourceProcessor::requestFinished(int status, const QByteArray &data, const QByteArray& errorString, int handle)
{

    if (status == 0)
    {
        qDebug() << "Successfully updated resource status" << data;
    } else
    {
        qDebug() << "Failed to update resource";
    }

}

void QnAppserverResourceProcessor::onResourceStatusChanged(const QnResourcePtr &resource)
{
    m_appServer->setResourceStatusAsync(resource->getId(), resource->getStatus(), this, SLOT(requestFinished(int,const QByteArray&,const QByteArray&, int)));
}
