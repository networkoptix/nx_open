#include "processor.h"

#include <QtCore/QThread>

#include <core/resource/camera_resource.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_discovery_manager.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(QnId serverId)
    : m_serverId(serverId)
{
    m_appServer = QnAppServerConnectionFactory::createConnection();
    connect(qnResPool, SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));

    QnAppServerConnectionFactory::ec2ConnectionFactory()->connectSync( QUrl(), &m_ec2Connection );
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
        if (cameras.isEmpty())
        {
            qCritical() << "QnAppserverResourceProcessor::processResources(): Call to addCamera failed. Unknown error code. Possible old ECS version is used!";
            continue;
        }

        QnVirtualCameraResourceListPtr cameraList2;
        const ec2::ErrorCode errorCode = m_ec2Connection->getCameraManager()->addCameraSync( cameraResource, &cameraList2 );
        if( errorCode != ec2::ErrorCode::ok )
            NX_LOG( QString::fromLatin1("Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );

        // cameras contains updated resource with all fields
        QnResourcePool::instance()->addResource(cameras.first());
    }
}

void QnAppserverResourceProcessor::updateResourceStatusAsync(const QnResourcePtr &resource)
{
    int handle = m_appServer->setResourceStatusAsync(resource->getId(), resource->getStatus(), this, "requestFinished");
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
    Q_ASSERT_X(!resource->hasFlags(QnResource::foreigner), Q_FUNC_INFO, "Status changed for foreign resource!");

    if (!isSetStatusInProgress(resource))
        updateResourceStatusAsync(resource);
    else
        m_awaitingSetStatus << resource;
}
