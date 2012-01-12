#include <QThread>

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

        QByteArray errorString;
        cameraResource->setParentId(m_serverId);
        if (m_appServer->addCamera(*cameraResource, cameras, errorString) != 0)
        {
            qDebug() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Reason: " << errorString;
        }

        qDebug() << "Connecting resource: " << resource->getName();
        QObject::connect(resource.data(), SIGNAL(onStatusChanged(QnResource::Status, QnResource::Status)), this, SLOT(onResourceStatusChanged(QnResource::Status, QnResource::Status)));
    }

    QnResourcePool::instance()->addResources(resources);
}

void QnAppserverResourceProcessor::requestFinished(int handle, int status, const QByteArray& errorString, const QnResourceList& resources)
{
    if (status == 0 && !resources.isEmpty())
    {
        qDebug() << "Successfully updated resource" << resources[0]->getName();
    } else
    {
        qDebug() << "Failed to update resource";
    }

}

void QnAppserverResourceProcessor::onResourceStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    QObject* xsender = sender();

    if (!xsender)
    {
        qDebug() << "QnAppserverResourceProcessor::onResourceStatusChanged() is not indended to be called directly";
        return;
    }

    QnResource* resource = dynamic_cast<QnResource*>(xsender);
    if (!resource)
    {
        qDebug() << "QnAppserverResourceProcessor::onResourceStatusChanged() should be connected to QnResource::onResourceStatusChanged() signal";
        return;
    }

    m_appServer->saveAsync(*resource, this, SLOT(requestFinished(int, int, const QByteArray&, const QnResourceList&)));
}
