#include "processor.h"

#include <QtCore/QThread>

#include <nx/utils/log/log.h>

#include <api/common_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>

#include "mutex/camera_data_handler.h"
#include "mutex/distributed_mutex_manager.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>

#include "media_server/serverutil.h"
#include "utils/common/util.h"
#include "core/resource/camera_user_attribute_pool.h"
#include "utils/license_usage_helper.h"
#include <media_server/settings.h>
#include <api/global_settings.h>


QnAppserverResourceProcessor::QnAppserverResourceProcessor(QnUuid serverId)
    : m_serverId(serverId)
{
    m_cameraDataHandler = new ec2::QnMutexCameraDataHandler();
    ec2::QnDistributedMutexManager::instance()->setUserDataHandler(m_cameraDataHandler);
    readDefaultUserAttrs();
}

QnAppserverResourceProcessor::~QnAppserverResourceProcessor()
{
    ec2::QnDistributedMutexManager::instance()->setUserDataHandler(0);
    delete m_cameraDataHandler;
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    for (const QnVirtualCameraResourcePtr& camera: resources.filtered<QnVirtualCameraResource>())
        camera->setParentId(m_serverId);

    // we've got two loops to avoid double call of double sending addCamera
    for (const QnVirtualCameraResourcePtr& camera: resources.filtered<QnVirtualCameraResource>())
    {
        // previous comment: camera MUST be in the pool already;
        // but now (new version) camera NOT in resource pool!

        auto secResource = camera.dynamicCast<QnSecurityCamResource>();

        QString urlStr = camera->getUrl();
        if (secResource && !secResource->getGroupId().isEmpty())
            urlStr = urlStr.left(urlStr.indexOf('?'));

        if (camera->isManuallyAdded() && !QnResourceDiscoveryManager::instance()->containManualCamera(urlStr))
            continue; //race condition. manual camera just deleted

        if( camera->hasFlags(Qn::search_upd_only) && !qnResPool->getResourceById(camera->getId()))
            continue;   //ignoring newly discovered camera

        QString uniqueId = camera->getUniqueId();
        camera->setId(camera->uniqueIdToId(uniqueId));
        addNewCamera(camera);
    }

}

void QnAppserverResourceProcessor::addNewCamera(const QnVirtualCameraResourcePtr& cameraResource)
{
    bool isOwnChangeParentId = cameraResource->hasFlags(Qn::parent_change) && cameraResource->preferedServerId() == qnCommon->moduleGUID(); // return camera back without mutex
    QnMediaServerResourcePtr ownServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    const bool takeCameraWithoutLock =
        (ownServer && (ownServer->getServerFlags() & Qn::SF_Edge) && !ownServer->isRedundancy()) ||
        qnGlobalSettings->takeCameraOwnershipWithoutLock() ||
        cameraResource->hasFlags(Qn::desktop_camera);
    if (!ec2::QnDistributedMutexManager::instance() || takeCameraWithoutLock || isOwnChangeParentId)
    {
        addNewCameraInternal(cameraResource);
        return;
    }

    QnMutexLocker lock( &m_mutex );
    QString name;
    if (cameraResource->hasFlags(Qn::parent_change))
        name = ec2::QnMutexCameraDataHandler::CAM_UPD_PREFIX;
    else
        name = ec2::QnMutexCameraDataHandler::CAM_INS_PREFIX;
    name.append(cameraResource->getPhysicalId());

    if (m_lockInProgress.contains(name))
        return; // camera already adding (in progress)

    if (!cameraResource->hasFlags(Qn::parent_change)) {
        if (!qnResPool->getAllNetResourceByPhysicalId(name).isEmpty())
            return; // already added. Camera has been found twice
    }

    ec2::QnDistributedMutex* mutex = ec2::QnDistributedMutexManager::instance()->createMutex(name);
    connect(mutex, &ec2::QnDistributedMutex::locked, this, &QnAppserverResourceProcessor::at_mutexLocked, Qt::QueuedConnection);
    connect(mutex, &ec2::QnDistributedMutex::lockTimeout, this, &QnAppserverResourceProcessor::at_mutexTimeout, Qt::QueuedConnection);
    m_lockInProgress.insert(name, LockData(mutex, cameraResource));
    mutex->lockAsync();
}

void QnAppserverResourceProcessor::at_mutexLocked()
{
    QnMutexLocker lock( &m_mutex );
    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    LockData data = m_lockInProgress.value(mutex->name());
    if (!data.mutex)
        return;

    if (mutex->checkUserData())
    {
        // add camera if and only if it absent on the other server
        NX_ASSERT(data.cameraResource->hasFlags(Qn::parent_change) || qnResPool->getAllNetResourceByPhysicalId(mutex->name()).isEmpty());
        addNewCameraInternal(data.cameraResource);
    }

    mutex->unlock();
    m_lockInProgress.remove(mutex->name());
    mutex->deleteLater();
}

void QnAppserverResourceProcessor::readDefaultUserAttrs()
{
    QString dir = MSSettings::roSettings()->value("staticDataDir", getDataDirectory()).toString();
    QFile f(closeDirPath(dir) + lit("default_rec.json"));
    if (!f.open(QFile::ReadOnly))
        return;
    QByteArray data = f.readAll();
    ec2::ApiCameraAttributesData userAttrsData;
    if (!QJson::deserialize(data, &userAttrsData))
        return;
    userAttrsData.preferedServerId = qnCommon->moduleGUID();
    m_defaultUserAttrs = QnCameraUserAttributesPtr(new QnCameraUserAttributes());
    fromApiToResource(userAttrsData, m_defaultUserAttrs);
}

ec2::ErrorCode QnAppserverResourceProcessor::addAndPropagateCamResource(
    const ec2::ApiCameraData& apiCameraData
)
{   // Add resource (camera) to the Resource Pool before launching transaction.
    // The order is important because while executing the transaction, specifically when
    // remote peer access rights are being checked, the resource should already reside in the
    // Resource Pool so ResourceAccessManager could check access rights for the pair
    // (remotePeerUserId, resourceId).
    QnResourcePtr existCamRes = qnResPool->getResourceById(apiCameraData.id);
    if (existCamRes && existCamRes->getTypeId() != apiCameraData.typeId)
        qnResPool->removeResource(existCamRes);
    QnCommonMessageProcessor::instance()->updateResource(apiCameraData);

    ec2::ErrorCode errorCode = QnAppServerConnectionFactory::getConnection2()
        ->getCameraManager(Qn::kDefaultUserAccess)
        ->addCameraSync(apiCameraData);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(
            QString::fromLatin1("Can't add camera to ec2 (insCamera query error). %1")
                .arg(ec2::toString(errorCode)),
            cl_logWARNING
        );
        // Here, if the transaction has failed, we have to restore Resource Pool
        // initial (before cameraResource processing began) state.
        qnResPool->removeResource(qnResPool->getResourceById(apiCameraData.id));
        if (existCamRes && existCamRes->getTypeId() != apiCameraData.typeId)
            QnCommonMessageProcessor::instance()->updateResource(existCamRes);
        return errorCode;
    }

    return ec2::ErrorCode::ok;
}

void QnAppserverResourceProcessor::addNewCameraInternal(const QnVirtualCameraResourcePtr& cameraResource) const
{
    bool resourceExists = static_cast<bool>(
        qnResPool->getResourceById(cameraResource->getId())
    );
    if (cameraResource->hasFlags(Qn::search_upd_only) && !resourceExists)
        return;   //ignoring newly discovered camera

    cameraResource->setFlags(cameraResource->flags() & ~Qn::parent_change);
    NX_ASSERT(!cameraResource->getId().isNull());

    ec2::ApiCameraData apiCamera;
    fromResourceToApi(cameraResource, apiCamera);

    if (addAndPropagateCamResource(apiCamera) == ec2::ErrorCode::ok)
    {   // finally, when transaction is successful we can save params
        // for our new resource.
        propertyDictionary->saveParams(cameraResource->getId());
    }

    if (!resourceExists && m_defaultUserAttrs)
    {
        QnCameraUserAttributesPtr userAttrCopy(new QnCameraUserAttributes(*m_defaultUserAttrs.data()));
        if (!userAttrCopy->scheduleDisabled) {
            QnCamLicenseUsageHelper helper;
            helper.propose(QnVirtualCameraResourceList() << cameraResource, true);
            if (!helper.isValid())
                userAttrCopy->scheduleDisabled = true;
        }
        userAttrCopy->cameraID = cameraResource->getId();

        ec2::ApiCameraAttributesDataList attrsList;
        fromResourceListToApi(QnCameraUserAttributesList() << userAttrCopy, attrsList);

        ec2::ErrorCode errCode =  QnAppServerConnectionFactory::getConnection2()->getCameraManager(Qn::kDefaultUserAccess)->saveUserAttributesSync(attrsList);
        if (errCode != ec2::ErrorCode::ok)
        {
            NX_LOG( QString::fromLatin1("Can't add camera to ec2 (insCamera user attributes query error). %1").arg(ec2::toString(errCode)), cl_logWARNING );
            return;
        }
        QSet<QByteArray> modifiedFields;
        {
            QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), userAttrCopy->cameraID );
            (*userAttributesLock)->assign( *userAttrCopy, &modifiedFields );
        }
        const QnResourcePtr& res = qnResPool->getResourceById(userAttrCopy->cameraID);
        if( res )   //it is OK if resource is missing
            res->emitModificationSignals( modifiedFields );
    }

    QnResourcePtr rpRes = qnResPool->getResourceById(cameraResource->getId());
    rpRes->setStatus(Qn::Offline);
    rpRes->initAsync(true);
}

void QnAppserverResourceProcessor::at_mutexTimeout()
{
    QnMutexLocker lock( &m_mutex );
    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    m_lockInProgress.remove(mutex->name());
    mutex->deleteLater();
}


bool QnAppserverResourceProcessor::isBusy() const
{
    bool rez = !m_lockInProgress.isEmpty();
    return rez;
}
