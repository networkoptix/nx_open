#include "processor.h"

#include "core/resourcemanagment/resource_pool.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(const QnId& serverId, QnResourceFactory& resourceFactory)
    : m_serverId(serverId)
{
    m_appServer = QnAppServerConnectionFactory::createConnection(resourceFactory);
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    QnResourceList cameras;

    foreach (QnResourcePtr resource, resources)
    {
        QnCameraResourcePtr cameraResource = resource.dynamicCast<QnCameraResource>();

        if (cameraResource.isNull())
            continue;

        m_appServer->addCamera(*cameraResource, m_serverId, cameras);
    }

    QnResourcePool::instance()->addResources(cameras);
}
