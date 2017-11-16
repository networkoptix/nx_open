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
    return
        [proc](Args... args)
        {
            if (proc)
                proc(args...);
        };
}

QPointer<QThread> threadGuard()
{
    return QThread::currentThread();
}

using ReplyProcessorFunction = std::function<void(int reqId, ec2::ErrorCode errorCode)>;

/**
 * Create handler that will be called in the callee thread and only if we have not changed the
 * actual connection session.
 */
ReplyProcessorFunction makeReplyProcessor(QnResourcesChangesManager* manager,
    ReplyProcessorFunction handler)
{
    QPointer<QnResourcesChangesManager> guard(manager);
    const auto sessionGuid = manager->commonModule()->runningInstanceGUID();
    QPointer<QThread> thread(QThread::currentThread());
    return
        [thread, guard, sessionGuid, handler](int reqID, ec2::ErrorCode errorCode)
        {
            if (!thread)
                return;

            executeInThread(thread,
                [guard, sessionGuid, reqID, errorCode, handler]
                {
                    if (!guard)
                        return;

                    // Check if we have already changed session.
                    if (guard->commonModule()->runningInstanceGUID() != sessionGuid)
                        return;

                    handler(reqID, errorCode);
                }); //< executeInThread
        };
}


template<typename ResourceType>
using ResourceCallbackFunction = std::function<void(bool, const QnSharedResourcePointer<ResourceType>&)>;

/**
* Create handler that will be called in the callee thread and only if we have not changed the
* actual connection session.
*/
template<typename ResourceType, typename BackupType>
ReplyProcessorFunction makeSaveResourceReplyProcessor(QnResourcesChangesManager* manager,
    QnSharedResourcePointer<ResourceType> resource,
    ResourceCallbackFunction<ResourceType> callback = ResourceCallbackFunction<ResourceType>())
{
    BackupType backup;
    ec2::fromResourceToApi(resource, backup);

    auto handler =
        [manager, resource, backup, callback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;

            if (success)
            {
                manager->resourcePool()->addNewResources({{resource}});
            }
            else
            {
                if (auto existing = manager->resourcePool()->getResourceById<ResourceType>(
                    resource->getId()))
                    {
                        ec2::fromApiToResource(backup, existing);
                    }
            }

            if (callback)
            {
                /* Resource could be added by transaction message bus, so shared pointer
                * will differ (if we have created a new resource). */
                auto updatedResource = manager->resourcePool()->getResourceById<ResourceType>(
                    resource->getId());

                callback(success, updatedResource);
            }

            if (!success)
                emit manager->saveChangesFailed({{resource}});
        };

    return makeReplyProcessor(manager, handler);
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
    const GenericCallbackFunction& callback)
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

    auto handler =
        [this, safeCallback, resources](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;

            // We don't want to wait until transaction is received.
            if (success)
                resourcePool()->removeResources(resources);

            safeCallback(success);

            if (!success)
                emit resourceDeletingFailed(resources);
        };

    QVector<QnUuid> idToDelete;
    for (const QnResourcePtr& resource: resources)
    {
        // if we are deleting an edge camera, also delete its server
        // check for camera to avoid unnecessary parent lookup
        QnUuid parentToDelete = resource.dynamicCast<QnVirtualCameraResource>() &&
            QnMediaServerResource::isHiddenServer(resource->getParentResource())
            ? resource->getParentId()
            : QnUuid();
        if (!parentToDelete.isNull())
            idToDelete << parentToDelete;
        idToDelete << resource->getId();
    }
    connection->getResourceManager(Qn::kSystemAccess)->remove(idToDelete, this,
        makeReplyProcessor(this, handler));
}


/************************************************************************/
/* Cameras block                                                        */
/************************************************************************/

void QnResourcesChangesManager::saveCamera(const QnVirtualCameraResourcePtr& camera,
    CameraChangesFunction applyChanges)
{
    saveCameras(QnVirtualCameraResourceList() << camera, applyChanges);
}

void QnResourcesChangesManager::saveCameras(const QnVirtualCameraResourceList& cameras,
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

void QnResourcesChangesManager::saveCamerasBatch(const QnVirtualCameraResourceList& cameras,
    GenericChangesFunction applyChanges,
    GenericCallbackFunction callback)
{
    if (cameras.isEmpty())
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto idList = idListFromResList(cameras);

    QPointer<QnCameraUserAttributePool> pool(cameraUserAttributesPool());

    QList<QnCameraUserAttributes> backup;
    for(const QnCameraUserAttributesPtr& cameraAttrs: pool->getAttributesList(idList))
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( pool, cameraAttrs->cameraId );
        backup << *(*userAttributesLock);
    }

    auto handler =
        [this, cameras, pool, backup, callback] (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            if (callback)
                callback(success);

            if (success)
                return;

            // Restore attributes from backup.
            for (const auto& cameraAttrs: backup)
            {
                QSet<QByteArray> modifiedFields;
                {
                    QnCameraUserAttributePool::ScopedLock userAttributesLock(pool, cameraAttrs.cameraId);
                    (*userAttributesLock)->assign(cameraAttrs, &modifiedFields);
                }

                // It is OK if resource is missing.
                if (const auto& res = resourcePool()->getResourceById(cameraAttrs.cameraId))
                    res->emitModificationSignals(modifiedFields);
            }

            emit saveChangesFailed(cameras);
        };

    if (applyChanges)
        applyChanges();
    auto changes = pool->getAttributesList(idList);
    ec2::ApiCameraAttributesDataList apiAttributes;
    fromResourceListToApi(changes, apiAttributes);
    connection->getCameraManager(Qn::kSystemAccess)->saveUserAttributes(apiAttributes, this,
        makeReplyProcessor(this, handler));

    // TODO: #GDM SafeMode values are not rolled back
    propertyDictionary()->saveParamsAsync(idList);
}

 void QnResourcesChangesManager::saveCamerasCore(const QnVirtualCameraResourceList& cameras,
     CameraChangesFunction applyChanges)
 {
     NX_EXPECT(applyChanges); //< ::saveCamerasCore is to be removed someday.
     if (!applyChanges)
         return;

     if (cameras.isEmpty())
         return;

     auto connection = commonModule()->ec2Connection();
     if (!connection)
         return;

     ec2::ApiCameraDataList backup;
     ec2::fromResourceListToApi(cameras, backup);

     auto handler =
        [this, cameras, backup](int /*reqID*/, ec2::ErrorCode errorCode)
        {
             /* Check if all OK */
             if (errorCode == ec2::ErrorCode::ok)
                 return;

             for (const ec2::ApiCameraData& data: backup)
             {
                 auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(data.id);
                 if (camera)
                     ec2::fromApiToResource(data, camera);
             }

             emit saveChangesFailed(cameras);
        };

     for (const auto& camera: cameras)
         applyChanges(camera);

     ec2::ApiCameraDataList apiCameras;
     ec2::fromResourceListToApi(cameras, apiCameras);
     connection->getCameraManager(Qn::kSystemAccess)->save(apiCameras, this,
         makeReplyProcessor(this, handler));
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
                                                 GenericChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

    if (servers.isEmpty())
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto idList = idListFromResList(servers);

    QPointer<QnMediaServerUserAttributesPool> pool(mediaServerUserAttributesPool());

    QList<QnMediaServerUserAttributes> backup;
    for(const auto& serverAttrs: pool->getAttributesList(idList))
    {
        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock(pool, serverAttrs->serverId);
        backup << *(*userAttributesLock);
    }

    auto handler =
        [this, servers, pool, backup] (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            /* Restore attributes from backup. */
            bool somethingWasChanged = false;
            for (const auto& serverAttrs: backup)
            {
                QSet<QByteArray> modifiedFields;
                {
                    QnMediaServerUserAttributesPool::ScopedLock userAttributesLock(pool, serverAttrs.serverId);
                    (*userAttributesLock)->assign(serverAttrs, &modifiedFields);
                }

                if (!modifiedFields.isEmpty())
                {
                    // It is OK if resource is missing.
                    if (const auto& res = resourcePool()->getResourceById(serverAttrs.serverId))
                        res->emitModificationSignals(modifiedFields);
                    somethingWasChanged = true;
                }
            }

            /* Silently die if nothing was changed. */
            if (!somethingWasChanged)
                return;

            emit saveChangesFailed(servers);
        };


    applyChanges();
    auto changes = pool->getAttributesList(idList);
    ec2::ApiMediaServerUserAttributesDataList attributes;
    fromResourceListToApi(changes, attributes);
    connection->getMediaServerManager(Qn::kSystemAccess)->saveUserAttributes(attributes, this,
        makeReplyProcessor(this, handler));

    // TODO: #GDM SafeMode values are not rolled back
    propertyDictionary()->saveParamsAsync(idList);
}

/************************************************************************/
/* Users block                                                          */
/************************************************************************/

void QnResourcesChangesManager::saveUser(const QnUserResourcePtr& user,
    UserChangesFunction applyChanges,
    UserCallbackFunction callback)
{
    const auto safeCallback = safeProcedure(callback);

    if (!applyChanges)
    {
        safeCallback(false, user);
        return;
    }

    NX_ASSERT(user);
    if (!user)
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
    {
        safeCallback(false, user);
        return;
    }

    auto replyProcessor = makeSaveResourceReplyProcessor<QnUserResource, ec2::ApiUserData>(this,
        user, callback);

    applyChanges(user);
    NX_ASSERT(!(user->isCloud() && user->getEmail().isEmpty()));
    ec2::ApiUserData apiUser;
    fromResourceToApi(user, apiUser);

    connection->getUserManager(Qn::kSystemAccess)->save(apiUser, user->getPassword(), this,
        replyProcessor);
}

void QnResourcesChangesManager::saveUsers(const QnUserResourceList& users)
{
    if (users.empty())
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    ec2::ApiUserDataList apiUsers;
    for (const auto& user: users)
    {
        apiUsers.push_back({});
        fromResourceToApi(user, apiUsers.back());
    }

    auto handler =
        [this, users](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            if (success)
                resourcePool()->addNewResources(users);
        };

    connection->getUserManager(Qn::kSystemAccess)->save(apiUsers, this,
        makeReplyProcessor(this, handler));
}

void QnResourcesChangesManager::saveAccessibleResources(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& accessibleResources)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto backup = sharedResourcesManager()->sharedResources(subject);
    if (backup == accessibleResources)
        return;

    sharedResourcesManager()->setSharedResources(subject, accessibleResources);

    auto handler =
        [this, subject, backup](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            if (!success)
                sharedResourcesManager()->setSharedResources(subject, backup);
        };

    ec2::ApiAccessRightsData accessRights;
    accessRights.userId = subject.effectiveId();
    for (const auto& id: accessibleResources)
        accessRights.resourceIds.push_back(id);
    connection->getUserManager(Qn::kSystemAccess)->setAccessRights(accessRights, this,
        makeReplyProcessor(this, handler));
}

void QnResourcesChangesManager::cleanAccessibleResources(const QnUuid& subject)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto handler = [this, subject](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {};

    ec2::ApiAccessRightsData accessRights;
    accessRights.userId = subject;
    connection->getUserManager(Qn::kSystemAccess)->setAccessRights(accessRights, this,
        makeReplyProcessor(this, handler));
}

void QnResourcesChangesManager::saveUserRole(const ec2::ApiUserRoleData& role)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto backup = userRolesManager()->userRole(role.id);
    userRolesManager()->addOrUpdateUserRole(role);

    auto handler =
        [this, backup, role](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            if (backup.id.isNull())
                userRolesManager()->removeUserRole(role.id);  /*< New group was not added */
            else
                userRolesManager()->addOrUpdateUserRole(backup);
        };

    connection->getUserManager(Qn::kSystemAccess)->saveUserRole(role, this,
        makeReplyProcessor(this, handler));
}

void QnResourcesChangesManager::removeUserRole(const QnUuid& id)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto backup = userRolesManager()->userRole(id);
    userRolesManager()->removeUserRole(id);

    auto handler =
        [this, backup](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            userRolesManager()->addOrUpdateUserRole(backup);
        };

    connection->getUserManager(Qn::kSystemAccess)->removeUserRole(id, this,
        makeReplyProcessor(this, handler));
}

void QnResourcesChangesManager::saveVideoWall(const QnVideoWallResourcePtr& videoWall,
    VideoWallChangesFunction applyChanges,
    VideoWallCallbackFunction callback)
{
    NX_ASSERT(videoWall);
    if (!videoWall)
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto replyProcessor = makeSaveResourceReplyProcessor<QnVideoWallResource, ec2::ApiVideowallData>(this,
        videoWall, callback);

    if (applyChanges)
        applyChanges(videoWall);
    ec2::ApiVideowallData apiVideowall;
    ec2::fromResourceToApi(videoWall, apiVideowall);

    connection->getVideowallManager(Qn::kSystemAccess)->save(apiVideowall, this,
        replyProcessor);
}

void QnResourcesChangesManager::saveLayout(const QnLayoutResourcePtr& layout,
    LayoutChangesFunction applyChanges,
    LayoutCallbackFunction callback)
{
    if (!applyChanges)
        return;

    NX_ASSERT(layout && !layout->isFile());
    if (!layout || layout->isFile())
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto replyProcessor = makeSaveResourceReplyProcessor<QnLayoutResource, ec2::ApiLayoutData>(this,
        layout, callback);

    applyChanges(layout);
    ec2::ApiLayoutData apiLayout;
    ec2::fromResourceToApi(layout, apiLayout);

    connection->getLayoutManager(Qn::kSystemAccess)->save(apiLayout, this, replyProcessor);
}

void QnResourcesChangesManager::saveWebPage(const QnWebPageResourcePtr& webPage,
    WebPageChangesFunction applyChanges,
    WebPageCallbackFunction callback)
{
    NX_ASSERT(webPage);
    if (!webPage)
        return;

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto replyProcessor = makeSaveResourceReplyProcessor<QnWebPageResource, ec2::ApiWebPageData>(this,
        webPage, callback);

    if (applyChanges)
        applyChanges(webPage);
    ec2::ApiWebPageData apiWebpage;
    ec2::fromResourceToApi(webPage, apiWebpage);

    connection->getWebPageManager(Qn::kSystemAccess)->save(apiWebpage, this, replyProcessor);
}
