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
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>

QnAppserverResourceProcessor::QnAppserverResourceProcessor(
    QnMediaServerModule* serverModule,
    ec2::QnDistributedMutexManager* distributedMutexManager,
    QnUuid serverId)
:
    nx::vms::server::ServerModuleAware(serverModule),
    m_serverId(serverId),
    m_distributedMutexManager(distributedMutexManager)
{
    if (m_distributedMutexManager)
    {
        m_cameraDataHandler = new ec2::QnMutexCameraDataHandler(serverModule->commonModule());
        m_distributedMutexManager->setUserDataHandler(m_cameraDataHandler);
    }
    readDefaultUserAttrs();
}

QnAppserverResourceProcessor::~QnAppserverResourceProcessor()
{
    if (m_distributedMutexManager)
        m_distributedMutexManager->setUserDataHandler(0);
    delete m_cameraDataHandler;
}

void QnAppserverResourceProcessor::processResources(const QnResourceList& resources)
{
    for (const QnVirtualCameraResourcePtr& camera: resources.filtered<QnVirtualCameraResource>())
    {
        NX_ASSERT(camera->resourcePool() == nullptr);
        camera->setParentId(m_serverId);
    }

    // we've got two loops to avoid double call of double sending addCamera
    for (const QnVirtualCameraResourcePtr& camera: resources.filtered<QnVirtualCameraResource>())
    {
        // previous comment: camera MUST be in the pool already;
        // but now (new version) camera NOT in resource pool!

        if (camera->isManuallyAdded() &&
            !serverModule()->commonModule()->resourceDiscoveryManager()->isManuallyAdded(camera))
        {
            continue; //race condition. manual camera just deleted
        }

        QString uniqueId = camera->getUniqueId();
        if( camera->hasFlags(Qn::search_upd_only) && !resourcePool()->getResourceByUniqueId(uniqueId))
            continue;   //ignoring newly discovered camera

        addNewCamera(camera);
    }

}

void QnAppserverResourceProcessor::addNewCamera(const QnVirtualCameraResourcePtr& cameraResource)
{
    bool isOwnChangeParentId = cameraResource->hasFlags(Qn::parent_change)
        && cameraResource->preferredServerId() == moduleGUID(); // return camera back without mutex
    QnMediaServerResourcePtr ownServer = resourcePool()->getResourceById<QnMediaServerResource>(moduleGUID());
    const bool takeCameraWithoutLock =
        (ownServer && ownServer->getServerFlags().testFlag(nx::vms::api::SF_Edge) && !ownServer->isRedundancy()) ||
        globalSettings()->takeCameraOwnershipWithoutLock() ||
        cameraResource->hasFlags(Qn::desktop_camera);
    if (!m_distributedMutexManager || takeCameraWithoutLock || isOwnChangeParentId)
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

    if (!cameraResource->hasFlags(Qn::parent_change))
    {
        if (resourcePool()->getNetResourceByPhysicalId(name))
            return; //< Already added. Camera has been found twice.
    }

    ec2::QnDistributedMutex* mutex = m_distributedMutexManager->createMutex(name);
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
        // Add camera if and only if it absent on the other server.
        NX_ASSERT(data.cameraResource->hasFlags(Qn::parent_change)
            || !resourcePool()->getNetResourceByPhysicalId(mutex->name()));
        addNewCameraInternal(data.cameraResource);
    }

    mutex->unlock();
    m_lockInProgress.remove(mutex->name());
    mutex->deleteLater();
}

void QnAppserverResourceProcessor::readDefaultUserAttrs()
{
    QString dir = serverModule()->settings().staticDataDir();
    QFile f(closeDirPath(dir) + lit("default_rec.json"));
    if (!f.open(QFile::ReadOnly))
        return;
    QByteArray data = f.readAll();
    nx::vms::api::CameraAttributesData userAttrsData;
    if (!QJson::deserialize(data, &userAttrsData))
        return;
    userAttrsData.preferredServerId = moduleGUID();
    m_defaultUserAttrs = QnCameraUserAttributesPtr(new QnCameraUserAttributes());
    ec2::fromApiToResource(userAttrsData, m_defaultUserAttrs);
}

ec2::ErrorCode QnAppserverResourceProcessor::addAndPropagateCamResource(
    QnCommonModule* commonModule,
    const nx::vms::api::CameraData& apiCameraData,
    const nx::vms::api::ResourceParamDataList& properties)
{
    QnResourcePtr existCamRes = commonModule->resourcePool()->getResourceById(apiCameraData.id);
    if (existCamRes && existCamRes->getTypeId() != apiCameraData.typeId)
        commonModule->resourcePool()->removeResource(existCamRes);

    /**
     * Save properties before camera to avoid race condition.
     * Camera will start initialization as soon as nx::vms::api::CameraData object will appear
     */

    ec2::ErrorCode errorCode = commonModule->ec2Connection()
        ->getResourceManager(Qn::kSystemAccess)
        ->saveSync(apiCameraData.id, properties);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_WARNING(typeid(QnAppserverResourceProcessor), QString::fromLatin1("Can't add camera's properties to database. DB error code %1").arg(ec2::toString(errorCode)));
        return errorCode;
    }

    errorCode = commonModule->ec2Connection()
        ->getCameraManager(Qn::kSystemAccess)
        ->addCameraSync(apiCameraData);
    if (errorCode != ec2::ErrorCode::ok)
        NX_WARNING(typeid(QnAppserverResourceProcessor), QString::fromLatin1("Can't add camera to database. DB error code %1").arg(ec2::toString(errorCode)));

    return errorCode;
}

void QnAppserverResourceProcessor::addNewCameraInternal(const QnVirtualCameraResourcePtr& cameraResource) const
{
    QString uniqueId = cameraResource->getUniqueId();
    bool resourceExists = static_cast<bool>(
        resourcePool()->getResourceByUniqueId(uniqueId)
    );
    if (cameraResource->hasFlags(Qn::search_upd_only) && !resourceExists)
        return;   //ignoring newly discovered camera

    cameraResource->setFlags(cameraResource->flags() & ~Qn::parent_change);

    nx::vms::api::CameraData apiCameraData;
    ec2::fromResourceToApi(cameraResource, apiCameraData);
    apiCameraData.id = cameraResource->physicalIdToId(uniqueId);

    ec2::ErrorCode errCode = addAndPropagateCamResource(
        serverModule()->commonModule(),
        apiCameraData,
        cameraResource->getRuntimeProperties());
    if (errCode != ec2::ErrorCode::ok)
        return;

    if (!resourceExists && m_defaultUserAttrs)
    {
        QnCameraUserAttributesPtr userAttrCopy(new QnCameraUserAttributes(*m_defaultUserAttrs.data()));
        if (userAttrCopy->licenseUsed)
        {
            QnCamLicenseUsageHelper helper(serverModule()->commonModule());
            helper.propose(QnVirtualCameraResourceList() << cameraResource, true);
            if (!helper.isValid())
                userAttrCopy->licenseUsed = false;
        }
        userAttrCopy->cameraId = apiCameraData.id;

        nx::vms::api::CameraAttributesDataList attrsList;
        ec2::fromResourceListToApi(QnCameraUserAttributesList() << userAttrCopy, attrsList);

        errCode =  ec2Connection()->getCameraManager(Qn::kSystemAccess)->saveUserAttributesSync(attrsList);
        if (errCode != ec2::ErrorCode::ok)
        {
            NX_WARNING (this, QString::fromLatin1("Can't add camera to ec2 (insCamera user attributes query error). %1").arg(ec2::toString(errCode)));
            return;
        }
        QSet<QByteArray> modifiedFields;
        {
            QnCameraUserAttributePool::ScopedLock userAttributesLock(cameraUserAttributesPool(), userAttrCopy->cameraId );
            (*userAttributesLock)->assign( *userAttrCopy, &modifiedFields );
        }
        const QnResourcePtr& res = resourcePool()->getResourceById(userAttrCopy->cameraId);
        if( res )   //it is OK if resource is missing
            res->emitModificationSignals( modifiedFields );
    }

    QnResourcePtr rpRes = resourcePool()->getResourceById(apiCameraData.id);
    if (rpRes)
    {
        rpRes->setStatus(Qn::Offline);
        rpRes->initAsync(true);
    }
}

void QnAppserverResourceProcessor::at_mutexTimeout()
{
    QnMutexLocker lock( &m_mutex );
    if (!sender())
    {
        NX_WARNING(this, lm("at_mutexTimeout: Sender is NULL"));
        return;
    }

    ec2::QnDistributedMutex* mutex = (ec2::QnDistributedMutex*) sender();
    m_lockInProgress.remove(mutex->name());
    mutex->deleteLater();
}

bool QnAppserverResourceProcessor::isBusy() const
{
    bool rez = !m_lockInProgress.isEmpty();
    return rez;
}
