// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_changes_manager.h"

#include <QtCore/QThread>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_media_server_manager.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

// TODO: #vkutin Move this function to some common helpers file.
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

using ReplyProcessorFunction = std::function<void(int reqId, ec2::ErrorCode errorCode)>;

/**
 * Create handler that will be called in the callee thread and only if we have not changed the
 * actual connection session.
 */
ReplyProcessorFunction makeReplyProcessor(ResourcesChangesManager* manager,
    ReplyProcessorFunction handler)
{
    QPointer<ResourcesChangesManager> guard(manager);
    const auto sessionGuid = manager->sessionId();
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
                    if (guard->sessionId() != sessionGuid)
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
ReplyProcessorFunction makeSaveResourceReplyProcessor(ResourcesChangesManager* manager,
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

ReplyProcessorFunction makeReplyProcessorUpdatePassword(
    ReplyProcessorFunction replyProcessor, QString newPassword)
{
    return
        [=](int reqId, ec2::ErrorCode errorCode)
        {
            auto currentSession = qnClientCoreModule->networkModule()->session();
            if (errorCode == ec2::ErrorCode::ok && currentSession)
                currentSession->updatePassword(newPassword);

            replyProcessor(reqId, errorCode);
        };
}

std::optional<QString> passwordIfUpdateIsRequired(
    QString currentUserName,
    const QnUserResourceList& users,
    QnUserResource::DigestSupport digestSupport)
{
    const auto currentUser = std::find_if(users.begin(), users.end(),
        [&](const QnUserResourcePtr& user)
        {
            return QString::compare(currentUserName, user->getName(), Qt::CaseInsensitive) == 0;
        });

    if (currentUser != users.end())
    {
        // After successfull call completion users.front()->getPassword() is empty, so saving it
        // here.
        const auto newPassword = (*currentUser)->getPassword();
        if (!newPassword.isEmpty() || digestSupport != QnUserResource::DigestSupport::keep)
            return newPassword;
    }

    return std::nullopt;
}

vms::api::UserData fromResourceToApi(
    QnUserResourcePtr user, QnUserResource::DigestSupport digestSupport)
{
    vms::api::UserData apiUser;
    ec2::fromResourceToApi(user, apiUser);

    if (digestSupport == QnUserResource::DigestSupport::disable)
        apiUser.digest = nx::vms::api::UserData::kHttpIsDisabledStub;
    if (digestSupport == QnUserResource::DigestSupport::enable && user->isLdap())
        apiUser.digest.clear();

    return apiUser;
}

template<class ResourcePtrType>
QList<QnUuid> idListFromResList(const QList<ResourcePtrType>& resList)
{
    QList<QnUuid> idList;
    idList.reserve(resList.size());
    for (const ResourcePtrType& resPtr : resList)
        idList.push_back(resPtr->getId());
    return idList;
}

} // namespace

ResourcesChangesManager::ResourcesChangesManager(QObject* parent):
    base_type(parent)
{}

ResourcesChangesManager::~ResourcesChangesManager()
{}

/************************************************************************/
/* Generic block                                                        */
/************************************************************************/

void ResourcesChangesManager::deleteResources(
    const QnResourceList& resources,
    const GenericCallbackFunction& callback)
{
    const auto safeCallback = safeProcedure(callback);

    if (resources.isEmpty())
    {
        safeCallback(false);
        return;
    }

    const auto connection = messageBusConnection();
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
            {
                NX_DEBUG(this, "About to remove resources");
                resourcePool()->removeResources(resources);
            }

            safeCallback(success);

            if (!success)
                emit resourceDeletingFailed(resources);
        };

    QVector<QnUuid> idToDelete;
    for (const QnResourcePtr& resource: resources)
    {
        // If we are deleting an edge camera, also delete its server.
        // Check for camera to avoid unnecessary parent lookup.
        QnUuid parentToDelete;
        if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            const bool isHiddenEdgeServer =
                QnMediaServerResource::isHiddenEdgeServer(camera->getParentResource());
            if (isHiddenEdgeServer && !camera->hasFlags(Qn::virtual_camera))
                parentToDelete = camera->getParentId();
        }

        if (!parentToDelete.isNull())
            idToDelete << parentToDelete; //< Parent remove its children by server side.
        else
            idToDelete << resource->getId();
    }
    connection->getResourceManager(Qn::kSystemAccess)->remove(
        idToDelete, makeReplyProcessor(this, handler), this);
}

/************************************************************************/
/* Cameras block                                                        */
/************************************************************************/

void ResourcesChangesManager::saveCamera(const QnVirtualCameraResourcePtr& camera,
    CameraChangesFunction applyChanges)
{
    saveCameras(QnVirtualCameraResourceList() << camera, applyChanges);
}

void ResourcesChangesManager::saveCameras(const QnVirtualCameraResourceList& cameras,
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

void ResourcesChangesManager::saveCamerasBatch(const QnVirtualCameraResourceList& cameras,
    GenericChangesFunction applyChanges,
    GenericCallbackFunction callback)
{
    if (cameras.isEmpty())
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    QList<std::pair<QnVirtualCameraResourcePtr, nx::vms::api::CameraAttributesData>> backup;
    for(const auto& camera: cameras)
        backup << std::pair(camera, camera->getUserAttributes());

    auto handler =
        [this, cameras, backup, callback] (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            if (callback)
                callback(success);

            if (success)
                return;

            // Restore attributes from backup.
            // TODO #lbusygin: Can we simplify code and do not apply changes to resource before
            // saveUserAttributes?
            for (const auto& cameraAttrs: backup)
                cameraAttrs.first->setUserAttributesAndNotify(cameraAttrs.second);

            emit saveChangesFailed(cameras);
        };

    if (applyChanges)
        applyChanges();

    nx::vms::api::CameraAttributesDataList changes;
    for(const auto& camera: cameras)
        changes.push_back(camera->getUserAttributes());

    connection->getCameraManager(Qn::kSystemAccess)->saveUserAttributes(
        changes, makeReplyProcessor(this, handler), this);

    // TODO: #sivanov Values are not rolled back in case of failure.
    auto idList = idListFromResList(cameras);
    resourcePropertyDictionary()->saveParamsAsync(idList);
}

void ResourcesChangesManager::saveCamerasCore(const QnVirtualCameraResourceList& cameras,
    CameraChangesFunction applyChanges)
{
    NX_ASSERT(applyChanges); //< ::saveCamerasCore is to be removed someday.
    if (!applyChanges)
        return;

    if (cameras.isEmpty())
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    nx::vms::api::CameraDataList backup;
    ec2::fromResourceListToApi(cameras, backup);

    auto handler =
       [this, cameras, backup](int /*reqID*/, ec2::ErrorCode errorCode)
       {
            /* Check if all OK */
            if (errorCode == ec2::ErrorCode::ok)
                return;

            for (const auto& data: backup)
            {
                auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(data.id);
                if (camera)
                    ec2::fromApiToResource(data, camera);
            }

            emit saveChangesFailed(cameras);
       };

    for (const auto& camera: cameras)
        applyChanges(camera);

    nx::vms::api::CameraDataList apiCameras;
    ec2::fromResourceListToApi(cameras, apiCameras);
    connection->getCameraManager(Qn::kSystemAccess)->addCameras(
        apiCameras, makeReplyProcessor(this, handler), this);
}

/************************************************************************/
/* Servers block                                                        */
/************************************************************************/

void ResourcesChangesManager::saveServer(const QnMediaServerResourcePtr &server,
    ServerChangesFunction applyChanges)
{
    saveServers(QnMediaServerResourceList() << server, applyChanges);
}

void ResourcesChangesManager::saveServers(const QnMediaServerResourceList &servers,
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

void ResourcesChangesManager::saveServersBatch(
    const QnMediaServerResourceList &servers,
    GenericChangesFunction applyChanges)
{
    if (!applyChanges)
        return;

    if (servers.isEmpty())
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    QList<std::pair<QnMediaServerResourcePtr, nx::vms::api::MediaServerUserAttributesData>> backup;
    for(const auto& server: servers)
        backup << std::pair(server, server->userAttributes());

    auto handler =
        [this, servers, backup] (int /*reqID*/, ec2::ErrorCode errorCode)
        {
            // Check if everithing is OK.
            if (errorCode == ec2::ErrorCode::ok)
                return;

            // Restore attributes from backup.
            bool somethingWasChanged = false;
            for (const auto& serverAttrs: backup)
            {
                somethingWasChanged = somethingWasChanged
                    || serverAttrs.first->setUserAttributesAndNotify(serverAttrs.second);
            }

            // Silently exit if nothing was changed.
            if (!somethingWasChanged)
                return;

            emit saveChangesFailed(servers);
        };

    applyChanges();

    nx::vms::api::MediaServerUserAttributesDataList changes;
    for(const auto& server: servers)
        changes.push_back(server->userAttributes());

    connection->getMediaServerManager(Qn::kSystemAccess)->saveUserAttributes(
        changes, makeReplyProcessor(this, handler), this);

    // TODO: #sivanov Values are not rolled back in case of failure.
    auto idList = idListFromResList(servers);
    resourcePropertyDictionary()->saveParamsAsync(idList);
}

/************************************************************************/
/* Users block                                                          */
/************************************************************************/

void ResourcesChangesManager::saveUser(const QnUserResourcePtr& user,
    QnUserResource::DigestSupport digestSupport,
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

    auto connection = messageBusConnection();
    if (!connection)
    {
        safeCallback(false, user);
        return;
    }

    auto replyProcessor = makeSaveResourceReplyProcessor<QnUserResource, vms::api::UserData>(this,
        user, callback);

    applyChanges(user);
    NX_ASSERT(!(user->isCloud() && user->getEmail().isEmpty()));

    const auto currentUserName = QString::fromStdString(connectionCredentials().username);
    if (auto password = passwordIfUpdateIsRequired(currentUserName, {user}, digestSupport))
        replyProcessor = makeReplyProcessorUpdatePassword(replyProcessor, *password);

    const auto apiUser = fromResourceToApi(user, digestSupport);
    connection->getUserManager(Qn::kSystemAccess)->save(apiUser, replyProcessor, this);
}

void ResourcesChangesManager::saveUsers(
    const QnUserResourceList& users, QnUserResource::DigestSupport digestSupport)
{
    if (users.empty())
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    vms::api::UserDataList apiUsers;
    for (const auto& user: users)
        apiUsers.push_back(fromResourceToApi(user, digestSupport));

    const auto newUsers = users.filtered(
        [](const QnResourcePtr& resource) { return resource->resourcePool() == nullptr; });

    resourcePool()->addNewResources(users);

    auto replyProcessor = makeReplyProcessor(this,
        [this, newUsers](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            if (!success)
            {
                NX_DEBUG(this,
                    "About to remove resources because of an error: %1",
                    errorCode);
                resourcePool()->removeResources(newUsers);
            }
        });

    const auto currentUserName = QString::fromStdString(connectionCredentials().username);
    if (auto password = passwordIfUpdateIsRequired(currentUserName, users, digestSupport))
        replyProcessor = makeReplyProcessorUpdatePassword(replyProcessor, *password);

    connection->getUserManager(Qn::kSystemAccess)->save(apiUsers, replyProcessor, this);
}

void ResourcesChangesManager::saveAccessibleResources(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& accessibleResources,
    GlobalPermissions permissions)
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    vms::api::AccessRightsData accessRights;
    accessRights.userId = subject.id();
    const auto resourceRights = nx::vms::api::globalPermissionsToAccessRights(permissions);
    for (const auto& id: accessibleResources)
        accessRights.resourceRights[id] = resourceRights;

    auto backup = sharedResourcesManager()->sharedResourceRights(subject);
    if (backup == accessRights.resourceRights)
        return;

    sharedResourcesManager()->setSharedResourceRights(subject, accessRights.resourceRights);

    auto handler =
        [this, subject, backup](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = errorCode == ec2::ErrorCode::ok;
            // Ignore setSharedResources() constraints here since we are just reverting the change.
            if (!success)
                sharedResourcesManager()->setSharedResourceRights(subject, backup);
        };

    connection->getUserManager(Qn::kSystemAccess)->setAccessRights(
        accessRights, makeReplyProcessor(this, handler), this);
}

void ResourcesChangesManager::saveAccessRights(
    const QnResourceAccessSubject& subject,
    const nx::core::access::ResourceAccessMap& accessRights)
{
    const auto connection = messageBusConnection();
    if (!connection)
        return;

    const auto accessRightsManager = systemContext()->accessRightsManager();
    const auto backup = accessRightsManager->ownResourceAccessMap(subject.id());

    if (backup == accessRights)
        return;

    accessRightsManager->setOwnResourceAccessMap(subject.id(), accessRights);

    auto handler =
        [this, subject, backup](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            if (const bool error = errorCode != ec2::ErrorCode::ok)
                systemContext()->accessRightsManager()->setOwnResourceAccessMap(subject.id(), backup);
        };

    vms::api::AccessRightsData accessRightsData;
    accessRightsData.userId = subject.id();
    accessRightsData.resourceRights.insert(
        accessRights.constKeyValueBegin(), accessRights.constKeyValueEnd());

    connection->getUserManager(Qn::kSystemAccess)->setAccessRights(
        accessRightsData, makeReplyProcessor(this, handler), this);
}

void ResourcesChangesManager::saveUserRole(const nx::vms::api::UserRoleData& role,
    RoleCallbackFunction callback)
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    auto backup = userRolesManager()->userRole(role.id);
    userRolesManager()->addOrUpdateUserRole(role);

    auto handler =
        [this, backup, role, callback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = (errorCode == ec2::ErrorCode::ok);
            if (!success)
            {
                if (backup.id.isNull())
                    userRolesManager()->removeUserRole(role.id);  //< New group was not added.
                else
                    userRolesManager()->addOrUpdateUserRole(backup);
            }
            if (callback)
                callback(success, role);
        };

    connection->getUserManager(Qn::kSystemAccess)->saveUserRole(
        role, makeReplyProcessor(this, handler), this);
}

void ResourcesChangesManager::removeUserRole(const QnUuid& id)
{
    auto connection = messageBusConnection();
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

    connection->getUserManager(Qn::kSystemAccess)->removeUserRole(
        id, makeReplyProcessor(this, handler), this);
}

void ResourcesChangesManager::saveVideoWall(const QnVideoWallResourcePtr& videoWall,
    VideoWallChangesFunction applyChanges,
    VideoWallCallbackFunction callback)
{
    NX_ASSERT(videoWall);
    if (!videoWall)
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    auto replyProcessor = makeSaveResourceReplyProcessor<
        QnVideoWallResource,
        nx::vms::api::VideowallData>(this, videoWall, callback);

    if (applyChanges)
        applyChanges(videoWall);
    nx::vms::api::VideowallData apiVideowall;
    ec2::fromResourceToApi(videoWall, apiVideowall);

    connection->getVideowallManager(Qn::kSystemAccess)->save(
        apiVideowall, replyProcessor, this);
}

void ResourcesChangesManager::saveWebPage(const QnWebPageResourcePtr& webPage,
    WebPageChangesFunction applyChanges,
    WebPageCallbackFunction callback)
{
    NX_ASSERT(webPage);
    if (!webPage)
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    auto replyProcessor = makeSaveResourceReplyProcessor<
        QnWebPageResource,
        nx::vms::api::WebPageData>(this, webPage, callback);

    if (applyChanges)
        applyChanges(webPage);
    nx::vms::api::WebPageData apiWebpage;
    ec2::fromResourceToApi(webPage, apiWebpage);

    connection->getWebPageManager(Qn::kSystemAccess)->save(apiWebpage, replyProcessor, this);

    // TODO: #sivanov Properties are not rolled back in case of failure.
    resourcePropertyDictionary()->saveParamsAsync({webPage->getId()});
}

} // namespace nx::vms::client::desktop
