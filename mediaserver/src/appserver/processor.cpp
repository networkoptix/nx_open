#include "processor.h"

#include <QtCore/QThread>

#include <core/resource/camera_resource.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "api/common_message_processor.h"
#include "mutex/camera_data_handler.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(QnId serverId)
    : m_serverId(serverId)
{
    connect(qnResPool, SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));

    connect(ec2::QnDistributedMutexManager::instance(), &ec2::QnDistributedMutexManager::locked, this, &QnAppserverResourceProcessor::at_mutexLocked);
    connect(ec2::QnDistributedMutexManager::instance(), &ec2::QnDistributedMutexManager::lockTimeout, this, &QnAppserverResourceProcessor::at_mutexTimeout);

    m_cameraDataHandler = new ec2::QnMutexCameraDataHandler();
    ec2::QnDistributedMutexManager::instance()->setUserDataHandler(m_cameraDataHandler);
}

QnAppserverResourceProcessor::~QnAppserverResourceProcessor()
{
    ec2::QnDistributedMutexManager::instance()->setUserDataHandler(0);
    delete m_cameraDataHandler;
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

        QString password = cameraResource->getAuth().password();


        if (cameraResource->isManuallyAdded() && !QnResourceDiscoveryManager::instance()->containManualCamera(cameraResource->getUrl()))
            continue; //race condition. manual camera just deleted
        /*
        QnVirtualCameraResourceList cameras;
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
        */
        addNewCamera(cameraResource);
    }
}

void QnAppserverResourceProcessor::addNewCamera(QnVirtualCameraResourcePtr cameraResource)
{
    if (!ec2::QnDistributedMutexManager::instance())
    {
        addNewCameraInternal(cameraResource);
        return;
    }

    QByteArray name = cameraResource->getPhysicalId().toUtf8();
    if (m_lockInProgress.contains(name))
        return; // camera already adding (in progress)

    ec2::QnDistributedMutexPtr mutex = ec2::QnDistributedMutexManager::instance()->getLock(name);
    m_lockInProgress.insert(name, LockData(mutex, cameraResource));
}

void QnAppserverResourceProcessor::at_mutexLocked(QByteArray name)
{
    LockData data = m_lockInProgress.value(name);
    if (!data.mutex) {
        Q_ASSERT_X(0, "It should not be!!!", Q_FUNC_INFO);
        return;
    }

    if (data.mutex->checkUserData())
    {
        // add camera if and only if it absent on the other server
        Q_ASSERT(qnResPool->getAllNetResourceByPhysicalId(name).isEmpty());
        addNewCameraInternal(data.cameraResource);
    }

    data.mutex->unlock();
    m_lockInProgress.remove(name);
}

void QnAppserverResourceProcessor::addNewCameraInternal(QnVirtualCameraResourcePtr cameraResource)
{
    QnVirtualCameraResourceList cameras;
    ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
    const ec2::ErrorCode errorCode = connect->getCameraManager()->addCameraSync( cameraResource, &cameras );
    if( errorCode == ec2::ErrorCode::ok )
        QnCommonMessageProcessor::instance()->updateResource(cameras.first());
    else
        NX_LOG( QString::fromLatin1("Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );
}

void QnAppserverResourceProcessor::at_mutexTimeout(QByteArray name)
{
    m_lockInProgress.remove(name);
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
