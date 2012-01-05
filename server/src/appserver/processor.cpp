#include "processor.h"

#include "core/resourcemanagment/resource_pool.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(const QnId& serverId)
    : m_serverId(serverId)
{
    m_appServer = QnAppServerConnectionFactory::createConnection();
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    QnResourceList cameras;

    foreach (QnResourcePtr resource, resources)
    {
        QnCameraResourcePtr cameraResource = resource.dynamicCast<QnCameraResource>();

        if (cameraResource.isNull())
            continue;

        cameraResource->setParentId(m_serverId);
        m_appServer->addCamera(*cameraResource, cameras);
    }

    QnResourcePool::instance()->addResources(cameras);
}
