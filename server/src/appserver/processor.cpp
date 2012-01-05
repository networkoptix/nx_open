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

        QByteArray errorString;
        cameraResource->setParentId(m_serverId);
        m_appServer->saveAsync(*cameraResource, this, SLOT(finished(int, int, const QByteArray&, const QnResourceList&)));

/*        if (m_appServer->addCamera(*cameraResource, cameras, errorString) != 0)
        {
            qDebug() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Reason: " << errorString;
        } */
    }


}

void QnAppserverResourceProcessor::finished(int handle, int status, const QByteArray& errorString, const QnResourceList& resources)
{
    qDebug() << "QnAppserverResourceProcessor::finished, h = " << handle << ", status = " << status << ", e = " << errorString << ", r = " << resources.size();

    QnResourcePool::instance()->addResources(resources);
}
