#include "processor.h"

#include <QtCore/QThread>

#include <utils/common/log.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>

#include "api/common_message_processor.h"
#include "mutex/camera_data_handler.h"
#include "mutex/distributed_mutex_manager.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(QUuid serverId)
    : m_serverId(serverId)
{
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
        QnVirtualCameraResource* cameraResource = dynamic_cast<QnVirtualCameraResource*>(resource.data());
        if (cameraResource == nullptr)
            continue;

        //Q_ASSERT(qnResPool->getAllNetResourceByPhysicalId(cameraResource->getPhysicalId()).isEmpty());

        cameraResource->setParentId(m_serverId);
        cameraResource->setPreferedServerId(m_serverId);
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
        resource->setStatus(Qn::Online, true); // set status in silence mode. Do not send any signals e.t.c

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
        QString uniqueId = cameraResource->getUniqueId();
        cameraResource->setId(cameraResource->uniqueIdToId(uniqueId));
        addNewCamera(cameraResource);
    }
}

void QnAppserverResourceProcessor::addNewCamera(const QnVirtualCameraResourcePtr& cameraResource)
{
    if (!ec2::QnDistributedMutexManager::instance())
    {
        addNewCameraInternal(cameraResource);
        return;
    }

    QMutexLocker lock(&m_mutex);
    QByteArray name = cameraResource->getPhysicalId().toUtf8();
    if (m_lockInProgress.contains(name))
        return; // camera already adding (in progress)

    if (!qnResPool->getAllNetResourceByPhysicalId(name).isEmpty())
        return; // already added. Camera has been found twice

    ec2::QnDistributedMutex* mutex = ec2::QnDistributedMutexManager::instance()->createMutex(name);
    connect(mutex, &ec2::QnDistributedMutex::locked, this, &QnAppserverResourceProcessor::at_mutexLocked, Qt::QueuedConnection);
    connect(mutex, &ec2::QnDistributedMutex::lockTimeout, this, &QnAppserverResourceProcessor::at_mutexTimeout, Qt::QueuedConnection);
    m_lockInProgress.insert(name, LockData(mutex, cameraResource));
    mutex->lockAsync();
}

void QnAppserverResourceProcessor::at_mutexLocked()
{
    QMutexLocker lock(&m_mutex);
    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    LockData data = m_lockInProgress.value(mutex->name());
    if (!data.mutex)
        return;

    if (mutex->checkUserData())
    {
        // add camera if and only if it absent on the other server
        Q_ASSERT(qnResPool->getAllNetResourceByPhysicalId(mutex->name()).isEmpty());
        addNewCameraInternal(data.cameraResource);
    }

    mutex->unlock();
    m_lockInProgress.remove(mutex->name());
    mutex->deleteLater();
}

void QnAppserverResourceProcessor::addNewCameraInternal(const QnVirtualCameraResourcePtr& cameraResource)
{
    Q_ASSERT(!cameraResource->getId().isNull());
    QnVirtualCameraResourceList cameras;
    ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
    const ec2::ErrorCode errorCode = connect->getCameraManager()->addCameraSync( cameraResource, &cameras );
    if( errorCode == ec2::ErrorCode::ok )
        QnCommonMessageProcessor::instance()->updateResource(cameras.first());
    else
        NX_LOG( QString::fromLatin1("Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );
}

void QnAppserverResourceProcessor::at_mutexTimeout()
{
    QMutexLocker lock(&m_mutex);
    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    m_lockInProgress.remove(mutex->name());
    mutex->deleteLater();
}


bool QnAppserverResourceProcessor::isBusy() const
{
    bool rez = !m_lockInProgress.isEmpty();
    return rez;
}
