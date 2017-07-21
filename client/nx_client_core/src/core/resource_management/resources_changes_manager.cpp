#include "resources_changes_manager.h"

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx_ec/data/api_conversion_functions.h>

#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_server_manager.h>

#include <utils/common/delayed.h>

namespace {

// TODO: #vkutin #common Move this function to some common helpers file
template<class... Args>
auto safeProcedure(std::function<void(Args...)> proc)
{
    return [proc](Args... args)
        {
            if (proc)
                proc(args...);
        };
}

} // namespace

QnResourcesChangesManager::QnResourcesChangesManager(QObject* parent):
    base_type(parent)
{}

QnResourcesChangesManager::~QnResourcesChangesManager()
{}

/************************************************************************/
/* Generic block                                                        */
/************************************************************************/

void QnResourcesChangesManager::deleteResources(
    const QnResourceList& resources,
    const ActionResultCallback& callback)
{
    const auto safeCallback = safeProcedure(callback);

    if (resources.isEmpty())
    {
        safeCallback(false);
        return;
    }

    const auto connection = commonModule()->ec2Connection();
    if (!connection)
    {
        safeCallback(false);
        return;
    }

    const auto sessionGuid = commonModule()->runningInstanceGUID();

    QVector<QnUuid> idToDelete;
    for(const QnResourcePtr& resource: resources) {
        // if we are deleting an edge camera, also delete its server
        QnUuid parentToDelete = resource.dynamicCast<QnVirtualCameraResource>() && //check for camera to avoid unnecessary parent lookup
            QnMediaServerResource::isHiddenServer(resource->getParentResource())
            ? resource->getParentId()
            : QnUuid();
        if (!parentToDelete.isNull())
            idToDelete << parentToDelete;
        idToDelete << resource->getId();
    }

    connection->getResourceManager(Qn::kSystemAccess)->remove(idToDelete, this,
        [this, safeCallback, resources, sessionGuid,
         thread = QPointer<QThread>(QThread::currentThread())]
            (int /* reqID */ , ec2::ErrorCode errorCode)
        {
            /* Check if we have already changed session: */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            const bool success = errorCode == ec2::ErrorCode::ok;

            // We don't want to wait until transaction is received.
            if (success)
                resourcePool()->removeResources(resources);

            if (thread)
                executeInThread(thread, [safeCallback, success]() { safeCallback(success); });

            if (!success)
                emit resourceDeletingFailed(resources);
        });
}


/************************************************************************/
/* Cameras block                                                        */
/************************************************************************/

void QnResourcesChangesManager::saveCamera(const QnVirtualCameraResourcePtr &camera,
    CameraChangesFunction applyChanges)
{
    saveCameras(QnVirtualCameraResourceList() << camera, applyChanges);
}

void QnResourcesChangesManager::saveCameras(const QnVirtualCameraResourceList &cameras,
    CameraChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

     auto batchFunction =
         [cameras, applyChanges]()
         {
             for (const QnVirtualCameraResourcePtr &camera: cameras)
                 applyChanges(camera);
         };
     saveCamerasBatch(cameras, batchFunction);
}

void QnResourcesChangesManager::saveCamerasBatch(const QnVirtualCameraResourceList &cameras,
                                                 BatchChangesFunction applyChanges,
                                                 RollbackFunction rollback)
{
    if (cameras.isEmpty())
        return;

    auto idList = idListFromResList(cameras);

    QPointer<QnCameraUserAttributePool> pool(cameraUserAttributesPool());

    QList<QnCameraUserAttributes> backup;
    for(const QnCameraUserAttributesPtr& cameraAttrs: pool->getAttributesList(idList))
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( pool, cameraAttrs->cameraId );
        backup << *(*userAttributesLock);
    }

    auto sessionGuid = commonModule()->runningInstanceGUID();

    if (applyChanges)
        applyChanges();
    auto changes = pool->getAttributesList(idList);

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    ec2::ApiCameraAttributesDataList apiAttributes;
    fromResourceListToApi(changes, apiAttributes);

    connection->getCameraManager(Qn::kSystemAccess)->saveUserAttributes(apiAttributes, this,
        [this, cameras, pool, backup, sessionGuid, rollback]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session or attributes pool was recreated. */
            if (!pool || commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            /* Restore attributes from backup. */
            for( const QnCameraUserAttributes& cameraAttrs: backup ) {
                QSet<QByteArray> modifiedFields;
                {
                    QnCameraUserAttributePool::ScopedLock userAttributesLock( pool, cameraAttrs.cameraId );
                    (*userAttributesLock)->assign( cameraAttrs, &modifiedFields );
                }
                if( const auto& res = resourcePool()->getResourceById(cameraAttrs.cameraId) )   //it is OK if resource is missing
                    res->emitModificationSignals( modifiedFields );
            }

            if (rollback)
                rollback();

            emit saveChangesFailed(cameras);
        });

    //TODO: #GDM SafeMode values are not rolled back
    propertyDictionary()->saveParamsAsync(idList);
}

 void QnResourcesChangesManager::saveCamerasCore(const QnVirtualCameraResourceList &cameras, CameraChangesFunction applyChanges) {
     if (!applyChanges)
         return;

     if (cameras.isEmpty())
         return;

     auto sessionGuid = commonModule()->runningInstanceGUID();

     ec2::ApiCameraDataList backup;
     ec2::fromResourceListToApi(cameras, backup);
     for (const QnVirtualCameraResourcePtr &camera: cameras)
         applyChanges(camera);

     auto connection = commonModule()->ec2Connection();
     if (!connection)
         return;

     ec2::ApiCameraDataList apiCameras;
     ec2::fromResourceListToApi(cameras, apiCameras);

     connection->getCameraManager(Qn::kSystemAccess)->save(apiCameras, this,
         [this, cameras, sessionGuid, backup]
         (int reqID, ec2::ErrorCode errorCode)
         {
             Q_UNUSED(reqID);

             /* Check if all OK */
             if (errorCode == ec2::ErrorCode::ok)
                 return;

             /* Check if we have already changed session or attributes pool was recreated. */
             if (commonModule()->runningInstanceGUID() != sessionGuid)
                 return;

             for (const ec2::ApiCameraData &data: backup) {
                 QnVirtualCameraResourcePtr camera = resourcePool()->getResourceById<QnVirtualCameraResource>(data.id);
                 if (camera)
                     ec2::fromApiToResource(data, camera);
             }

             emit saveChangesFailed(cameras);
         });
}


/************************************************************************/
/* Servers block                                                        */
/************************************************************************/

void QnResourcesChangesManager::saveServer(const QnMediaServerResourcePtr &server,
    ServerChangesFunction applyChanges)
{
    saveServers(QnMediaServerResourceList() << server, applyChanges);
}

void QnResourcesChangesManager::saveServers(const QnMediaServerResourceList &servers,
    ServerChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

    auto batchFunction =
        [servers, applyChanges]()
        {
            for (const QnMediaServerResourcePtr &server: servers)
                applyChanges(server);
        };
    saveServersBatch(servers, batchFunction);
}

void QnResourcesChangesManager::saveServersBatch(const QnMediaServerResourceList &servers,
                                                 BatchChangesFunction applyChanges,
                                                 RollbackFunction rollback)
{
    if (!applyChanges)
        return;

    if (servers.isEmpty())
        return;

    auto idList = idListFromResList(servers);

    QPointer<QnMediaServerUserAttributesPool> pool(mediaServerUserAttributesPool());

    QList<QnMediaServerUserAttributes> backup;
    for(const QnMediaServerUserAttributesPtr& serverAttrs: pool->getAttributesList(idList)) {
        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( pool, serverAttrs->serverId );
        backup << *(*userAttributesLock);
    }

    auto sessionGuid = commonModule()->runningInstanceGUID();

    applyChanges();
    auto changes = pool->getAttributesList(idList);

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    ec2::ApiMediaServerUserAttributesDataList attributes;
    fromResourceListToApi(changes, attributes);

    connection->getMediaServerManager(Qn::kSystemAccess)->saveUserAttributes(attributes, this,
        [this, servers, pool, backup, sessionGuid, rollback]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session or attributes pool was recreated. */
            if (!pool || commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            /* Restore attributes from backup. */
            bool somethingWasChanged = false;
            for( const QnMediaServerUserAttributes& serverAttrs: backup ) {
                QSet<QByteArray> modifiedFields;
                {
                    QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( pool, serverAttrs.serverId);
                    (*userAttributesLock)->assign( serverAttrs, &modifiedFields );
                }
                if (!modifiedFields.isEmpty()) {
                    if( const QnResourcePtr& res = resourcePool()->getResourceById(serverAttrs.serverId) )   //it is OK if resource is missing
                        res->emitModificationSignals( modifiedFields );
                    somethingWasChanged = true;
                }
            }

            if (rollback)
                rollback();

            /* Silently die if nothing was changed. */
            if (!somethingWasChanged)
                return;

            emit saveChangesFailed(servers);
        });

    //TODO: #GDM SafeMode values are not rolled back
    propertyDictionary()->saveParamsAsync(idList);
}

/************************************************************************/
/* Users block                                                          */
/************************************************************************/

void QnResourcesChangesManager::saveUser(const QnUserResourcePtr& user,
    UserChangesFunction applyChanges, const ActionResultCallback& callback)
{
    const auto safeCallback = safeProcedure(callback);

    if (!applyChanges)
    {
        safeCallback(false);
        return;
    }

    NX_ASSERT(user);
    if (!user)
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
    {
        safeCallback(false);
        return;
    }

    auto sessionGuid = commonModule()->runningInstanceGUID();

    ec2::ApiUserData backup;
    ec2::fromResourceToApi(user, backup);

    applyChanges(user);
    QnUuid userId = user->getId();

    NX_ASSERT(!(user->isCloud() && user->getEmail().isEmpty()));

    ec2::ApiUserData apiUser;
    fromResourceToApi(user, apiUser);

    connection->getUserManager(Qn::kSystemAccess)->save(apiUser, user->getPassword(), this,
        [this, user, userId, sessionGuid, backup, safeCallback,
         thread = QPointer<QThread>(QThread::currentThread())]
            (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if we have already changed session: */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            const bool success = errorCode == ec2::ErrorCode::ok;

            if (thread)
                executeInThread(thread, [safeCallback, success]() { safeCallback(success); });

            if (success)
                return;

            QnUserResourcePtr existingUser = resourcePool()->getResourceById<QnUserResource>(userId);
            if (existingUser)
                ec2::fromApiToResource(backup, existingUser);

            emit saveChangesFailed(QnResourceList() << user);
        });
}

void QnResourcesChangesManager::saveUsers(const QnUserResourceList& users)
{
    if (users.empty())
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();

    ec2::ApiUserDataList apiUsers;
    for (const auto& user: users)
    {
        apiUsers.push_back({});
        fromResourceToApi(user, apiUsers.back());
    }

    connection->getUserManager(Qn::kSystemAccess)->save(apiUsers, this,
        [](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {});
}

void QnResourcesChangesManager::saveAccessibleResources(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& accessibleResources)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();
    auto backup = sharedResourcesManager()->sharedResources(subject);
    if (backup == accessibleResources)
        return;

    sharedResourcesManager()->setSharedResources(subject, accessibleResources);

    ec2::ApiAccessRightsData accessRights;
    accessRights.userId = subject.effectiveId();
    for (const auto &id : accessibleResources)
        accessRights.resourceIds.push_back(id);
    connection->getUserManager(Qn::kSystemAccess)->setAccessRights(accessRights, this,
        [this, subject, sessionGuid, backup]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session. */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            sharedResourcesManager()->setSharedResources(subject, backup);
        });
}

void QnResourcesChangesManager::saveUserRole(const ec2::ApiUserRoleData& role)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();

    auto backup = userRolesManager()->userRole(role.id);
    userRolesManager()->addOrUpdateUserRole(role);

    connection->getUserManager(Qn::kSystemAccess)->saveUserRole(role, this,
        [this, backup, role, sessionGuid]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session. */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            if (backup.id.isNull())
                userRolesManager()->removeUserRole(role.id);  /*< New group was not added */
            else
                userRolesManager()->addOrUpdateUserRole(backup);
        });
}

void QnResourcesChangesManager::removeUserRole(const QnUuid& id)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();

    auto backup = userRolesManager()->userRole(id);
    userRolesManager()->removeUserRole(id);

    connection->getUserManager(Qn::kSystemAccess)->removeUserRole(id, this,
        [this, backup, sessionGuid]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {

            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session. */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            userRolesManager()->addOrUpdateUserRole(backup);
        });
}

/************************************************************************/
/* VideoWalls block                                                     */
/************************************************************************/
//TODO: #GDM #Safe Mode
void QnResourcesChangesManager::saveVideoWall(const QnVideoWallResourcePtr &videoWall,
    VideoWallChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

    NX_ASSERT(videoWall);
    if (!videoWall)
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();

    ec2::ApiVideowallData backup;
    ec2::fromResourceToApi(videoWall, backup);


    applyChanges(videoWall);

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    ec2::ApiVideowallData apiVideowall;
    ec2::fromResourceToApi(videoWall, apiVideowall);
    connection->getVideowallManager(Qn::kSystemAccess)->save(apiVideowall, this,
        [this, videoWall, sessionGuid, backup]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session or attributes pool was recreated. */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            QnUuid videoWallId = videoWall->getId();
            QnVideoWallResourcePtr existingVideoWall = resourcePool()->getResourceById<QnVideoWallResource>(videoWallId);
            if (existingVideoWall)
                ec2::fromApiToResource(backup, existingVideoWall);

            emit saveChangesFailed(QnResourceList() << videoWall);
        });
}

/************************************************************************/
/* Layouts block                                                        */
/************************************************************************/

void QnResourcesChangesManager::saveLayout(const QnLayoutResourcePtr &layout,
    LayoutChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

    NX_ASSERT(layout);
    if (!layout)
        return;

    NX_ASSERT(!layout->isFile());
    if (layout->isFile())
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();

    ec2::ApiLayoutData backup;
    ec2::fromResourceToApi(layout, backup);

    applyChanges(layout);

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    ec2::ApiLayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    connection->getLayoutManager(Qn::kSystemAccess)->save(apiLayout, this,
        [this, layout, sessionGuid, backup]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session or attributes pool was recreated. */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            QnUuid layoutId = layout->getId();
            QnLayoutResourcePtr existingLayout = resourcePool()->getResourceById<QnLayoutResource>(layoutId);
            if (existingLayout)
                ec2::fromApiToResource(backup, existingLayout);

            emit saveChangesFailed(QnResourceList() << layout);
        });
}

/************************************************************************/
/* WebPages block                                                       */
/************************************************************************/

void QnResourcesChangesManager::saveWebPage(const QnWebPageResourcePtr &webPage,
    WebPageChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

    NX_ASSERT(webPage);
    if (!webPage)
        return;

    auto sessionGuid = commonModule()->runningInstanceGUID();

    ec2::ApiWebPageData backup;
    ec2::fromResourceToApi(webPage, backup);

    applyChanges(webPage);

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    ec2::ApiWebPageData apiWebpage;
    ec2::fromResourceToApi(webPage, apiWebpage);

    connection->getWebPageManager(Qn::kSystemAccess)->save(apiWebpage, this,
        [this, webPage, sessionGuid, backup]
        (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Check if we have already changed session or attributes pool was recreated. */
            if (commonModule()->runningInstanceGUID() != sessionGuid)
                return;

            QnUuid webPageId = webPage->getId();
            QnWebPageResourcePtr existingWebPage = resourcePool()->getResourceById<QnWebPageResource>(webPageId);
            if (existingWebPage)
                ec2::fromApiToResource(backup, existingWebPage);

            emit saveChangesFailed(QnResourceList() << webPage);
        });
}

