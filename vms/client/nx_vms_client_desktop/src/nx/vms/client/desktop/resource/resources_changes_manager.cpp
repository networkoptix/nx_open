// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_changes_manager.h"

#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/api/data/server_model.h>
#include <nx/vms/api/data/user_group_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/window_context.h>
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
#include <ui/workbench/workbench_context.h>
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
using ReplyProcessorFunctionRest = std::function<void(
    bool success, rest::Handle requestId, rest::ServerConnection::ErrorOrEmpty result)>;

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

ReplyProcessorFunctionRest makeReplyProcessorRest(ResourcesChangesManager* manager,
    ReplyProcessorFunctionRest handler)
{
    QPointer<ResourcesChangesManager> guard(manager);
    const auto sessionGuid = manager->sessionId();
    QPointer<QThread> thread(QThread::currentThread());
    return
        [thread, guard, sessionGuid, handler](
            bool success, rest::Handle requestId, rest::ServerConnection::ErrorOrEmpty result)
        {
            if (!thread)
                return;

            executeInThread(thread,
                [guard, sessionGuid, success, requestId, result, handler]
                {
                    if (!guard)
                        return;

                    // Check if we have already changed session.
                    if (guard->sessionId() != sessionGuid)
                        return;

                    handler(success, requestId, result);
                }); //< executeInThread
        };
    }

template<typename ResourceType>
using ResourceCallbackFunction =
    std::function<void(bool, const QnSharedResourcePointer<ResourceType>&)>;

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
    SystemContext* systemContext, ReplyProcessorFunction replyProcessor, QString newPassword)
{
    return
        [=](int reqId, ec2::ErrorCode errorCode)
        {
            auto currentSession = systemContext->session();
            if (errorCode == ec2::ErrorCode::ok && currentSession)
                currentSession->updatePassword(newPassword);

            replyProcessor(reqId, errorCode);
        };
}

template<typename ResourceType, typename BackupType>
ReplyProcessorFunctionRest makeSaveResourceReplyProcessorRest(ResourcesChangesManager* manager,
    QnSharedResourcePointer<ResourceType> resource,
    ResourceCallbackFunction<ResourceType> callback = ResourceCallbackFunction<ResourceType>())
{
    BackupType backup;
    ec2::fromResourceToApi(resource, backup);

    auto handler =
        [manager, resource, backup, callback](bool success,
            rest::Handle requestId,
            rest::ServerConnection::ErrorOrEmpty result)
        {
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
                // Resource could be added by transaction message bus, so shared pointer
                // will differ (if we have created a new resource).
                auto updatedResource = manager->resourcePool()->getResourceById<ResourceType>(
                    resource->getId());

                callback(success, updatedResource);
            }

            if (!success)
                emit manager->saveChangesFailed({{resource}});
        };

    return makeReplyProcessorRest(manager, handler);
}

ReplyProcessorFunctionRest makeReplyProcessorUpdatePasswordRest(
    SystemContext* systemContext, ReplyProcessorFunctionRest replyProcessor, QString newPassword)
{
    return
        [=](bool success, rest::Handle requestId, rest::ServerConnection::ErrorOrEmpty result)
        {
            auto currentSession = systemContext->session();
            if (success && currentSession)
                currentSession->updatePassword(newPassword);

            replyProcessor(success, requestId, result);
        };
}

std::optional<QString> passwordIfUpdateIsRequired(QString currentUserName,
    const QnUserResourceList& users,
    QnUserResource::DigestSupport digestSupport)
{
    const auto currentUser = std::find_if(users.begin(),
        users.end(),
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
    for (const ResourcePtrType& resPtr: resList)
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

void ResourcesChangesManager::deleteResource(const QnResourcePtr& resource,
    const DeleteResourceCallbackFunction& callback,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    const auto safeCallback = safeProcedure(callback);

    if (!resource)
    {
        safeCallback(false, resource);
        return;
    }

    auto api = connectedServerApi();
    if (!api)
    {
        safeCallback(false, resource);
        return;
    }

    auto handler =
        [this, safeCallback, resource](bool success,
            rest::Handle /*requestId*/,
            rest::ServerConnection::ErrorOrEmpty /*result*/)
        {
            if (success)
            {
                NX_DEBUG(this, "About to remove user");
                resourcePool()->removeResource(resource);
            }

            safeCallback(success, resource);

            if (!success)
                emit resourceDeletingFailed({resource});
        };

    QString action;
    if (resource.dynamicCast<QnMediaServerResource>())
    {
        action = QString("/rest/v3/servers/%1").arg(resource->getId().toString());
    }
    else if (resource.dynamicCast<QnVirtualCameraResource>())
    {
        action = QString("/rest/v3/devices/%1").arg(resource->getId().toString());
    }
    else if (resource.dynamicCast<QnStorageResource>())
    {
        action = QString("/rest/v3/servers/%1/storages/%2")
                     .arg(resource->getParentId().toString(), resource->getId().toString());
    }
    else if (resource.dynamicCast<QnUserResource>())
    {
        action = QString("/rest/v3/users/%1").arg(resource->getId().toString());
    }
    else if (resource.dynamicCast<QnLayoutResource>())
    {
        action = QString("/rest/v3/layouts/%1").arg(resource->getId().toString());
    }
    else if (resource.dynamicCast<QnVideoWallResource>())
    {
        action = QString("/rest/v3/videowalls/%1").arg(resource->getId().toString());
    }
    else if (resource.dynamicCast<QnWebPageResource>())
    {
        action = QString("/rest/v3/webPages/%1").arg(resource->getId().toString());
    }

    const auto systemContext = SystemContext::fromResource(resource);
    const auto tokenHelper = helper
        ? helper
        : systemContext->restApiHelper()->getSessionTokenHelper();

    api->deleteRest(tokenHelper, action, network::rest::Params{}, handler, thread());
}

void ResourcesChangesManager::deleteResources(const QnResourceList& resources,
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
            for (const QnVirtualCameraResourcePtr& camera: cameras)
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
    for (const auto& camera: cameras)
        backup << std::pair(camera, camera->getUserAttributes());

    auto handler =
        [this, cameras, backup, callback](int /*reqID*/, ec2::ErrorCode errorCode)
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
    for (const auto& camera: cameras)
        changes.push_back(camera->getUserAttributes());

    connection->getCameraManager(Qn::kSystemAccess)->saveUserAttributes(
        changes, makeReplyProcessor(this, handler), this);

    // TODO: #sivanov Values are not rolled back in case of failure.
    auto idList = idListFromResList(cameras);
    resourcePropertyDictionary()->saveParamsAsync(idList);
}

void ResourcesChangesManager::saveCamerasCore(
    const QnVirtualCameraResourceList& cameras, CameraChangesFunction applyChanges)
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

void ResourcesChangesManager::saveServer(const QnMediaServerResourcePtr& server,
    ServerChangesFunction applyChanges,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    if (!applyChanges)
        return;

    if (!server)
        return;

    nx::vms::api::MediaServerUserAttributesData backup = server->userAttributes();

    auto restore =
        [this, server, backup]
        {
            // Restore attributes from backup.
            if (!server->setUserAttributesAndNotify(backup))
                return;

            emit saveChangesFailed(QnResourceList() << server);
        };

    applyChanges(server);

    const auto systemContext = SystemContext::fromResource(server);
    if (systemContext->restApiHelper()->restApiEnabled())
    {
        auto api = connectedServerApi();
        if (!api)
        {
            restore();
            return;
        }

        auto handler =
            [restore](bool success,
                rest::Handle /*requestId*/,
                rest::ServerConnection::ErrorOrEmpty /*result*/)
            {
                if (success)
                    return;

                restore();
            };

        nx::vms::api::ServerModel body;

        auto change = server->userAttributes();
        body.id = change.serverId;
        body.name = change.serverName;
        body.url = server->getUrl();
        auto modifiedProperties = resourcePropertyDictionary()->modifiedProperties(server->getId());

        std::map<QString, QJsonValue> result;

        auto itr = modifiedProperties.find(server->getId());
        if (itr != modifiedProperties.end())
        {
            const QMap<QString, QString>& properties = itr.value();
            for (auto itr = properties.begin(); itr != properties.end(); ++itr)
                result[itr.key()] = QJsonValue(itr.value());
        }

        body.parameters = result;

        const auto tokenHelper = helper
            ? helper
            : systemContext->restApiHelper()->getSessionTokenHelper();

        api->putRest(tokenHelper,
            QString("/rest/v3/servers/%1").arg(server->getId().toString()),
            network::rest::Params{},
            QJson::serialized(body),
            handler,
            thread());
    }
    else
    {
        auto connection = messageBusConnection();
        if (!connection)
        {
            restore();
            return;
        }

        auto handler =
            [restore](int /*reqID*/, ec2::ErrorCode errorCode)
            {
                // Check if everithing is OK.
                if (errorCode == ec2::ErrorCode::ok)
                    return;

                restore();
            };
        nx::vms::api::MediaServerUserAttributesDataList changes;
        changes.push_back(server->userAttributes());
        connection->getMediaServerManager(Qn::kSystemAccess)
            ->saveUserAttributes(changes, makeReplyProcessor(this, handler), this);

        // TODO: #sivanov Values are not rolled back in case of failure.
        auto idList = idListFromResList(QnResourceList() << server);
        resourcePropertyDictionary()->saveParamsAsync(idList);
    }
}

/************************************************************************/
/* Users block                                                          */
/************************************************************************/

void ResourcesChangesManager::saveUser(const QnUserResourcePtr& user,
    QnUserResource::DigestSupport digestSupport,
    UserChangesFunction applyChanges,
    SystemContext* systemContext,
    UserCallbackFunction callback,
    const nx::vms::common::SessionTokenHelperPtr& helper)
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

    if (const auto mock = mockImplementation())
    {
        applyChanges(user);
        const bool success = mock->saveUser(user);
        safeCallback(success, user);
        return;
    }

    if (systemContext->restApiHelper()->restApiEnabled())
    {
        auto api = connectedServerApi();
        if (!api)
        {
            safeCallback(false, user);
            return;
        }

        const bool isNewUser = user->resourcePool() == nullptr;

        auto replyProcessor =
            makeSaveResourceReplyProcessorRest<QnUserResource, vms::api::UserData>(
                this, user, callback);

        applyChanges(user);
        NX_ASSERT(!(user->isCloud() && user->getEmail().isEmpty()));

        const auto currentUserName = QString::fromStdString(connectionCredentials().username);
        if (auto password = passwordIfUpdateIsRequired(currentUserName, {user}, digestSupport))
        {
            replyProcessor =
                makeReplyProcessorUpdatePasswordRest(systemContext, replyProcessor, *password);
        }

        nx::vms::api::UserModelV3 body;
        body.id = user->getId();
        body.name = user->getName();
        body.email = user->getEmail();
        body.type = user->userType();
        body.fullName = user->fullName();
        body.permissions = user->getRawPermissions();
        body.isEnabled = user->isEnabled();
        switch (digestSupport)
        {
            case QnUserResource::DigestSupport::enable:
                body.isHttpDigestEnabled = true;
                break;
            case QnUserResource::DigestSupport::disable:
                body.isHttpDigestEnabled = false;
                break;
            default:
                body.isHttpDigestEnabled =
                    user->getDigest() != nx::vms::api::UserData::kHttpIsDisabledStub;
        }
        if (const auto password = user->getPassword(); !password.isEmpty())
            body.password = password;

        body.groupIds = user->userRoleIds();

        const auto accessRights = systemContext->accessRightsManager()->ownResourceAccessMap(
            user->getId());

        body.resourceAccessRights = {accessRights.keyValueBegin(), accessRights.keyValueEnd()};

        const auto tokenHelper = helper
            ? helper
            : systemContext->restApiHelper()->getSessionTokenHelper();

        if (isNewUser)
        {
            NX_ASSERT(body.password.has_value());

            api->putRest(tokenHelper,
                QString("/rest/v3/users/%1").arg(user->getId().toString()),
                network::rest::Params{},
                nx::reflect::json::serialize(body).c_str(),
                replyProcessor,
                thread());
        }
        else
        {
            api->patchRest(tokenHelper,
                QString("/rest/v3/users/%1").arg(user->getId().toString()),
                network::rest::Params{},
                nx::reflect::json::serialize(body).c_str(),
                replyProcessor,
                thread());
        }
    }
    else
    {
        auto connection = messageBusConnection();
        if (!connection)
        {
            safeCallback(false, user);
            return;
        }

        auto replyProcessor = makeSaveResourceReplyProcessor<QnUserResource, vms::api::UserData>(
            this, user, callback);

        applyChanges(user);
        NX_ASSERT(!(user->isCloud() && user->getEmail().isEmpty()));

        const auto currentUserName = QString::fromStdString(connectionCredentials().username);
        if (auto password = passwordIfUpdateIsRequired(currentUserName, {user}, digestSupport))
        {
            replyProcessor =
                makeReplyProcessorUpdatePassword(systemContext, replyProcessor, *password);
        }

        const auto apiUser = fromResourceToApi(user, digestSupport);
        connection->getUserManager(Qn::kSystemAccess)->save(apiUser, replyProcessor, this);
    }
}

void ResourcesChangesManager::saveUsers(const QnUserResourceList& users,
    QnUserResource::DigestSupport digestSupport,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    if (users.empty())
        return;

    const auto systemContext = SystemContext::fromResource(users.front());
    if (systemContext->restApiHelper()->restApiEnabled())
    {
        auto api = connectedServerApi();
        if (!api)
            return;

        const auto newUsers = users.filtered(
            [](const QnResourcePtr& resource) { return resource->resourcePool() == nullptr; });

        std::set<QnUuid> newUsersSet;
        for (const auto& user: newUsers)
            newUsersSet.insert(user->getId());

        resourcePool()->addNewResources(users);

        for (const auto& user: users)
        {
            auto replyProcessor = makeReplyProcessorRest(this,
                [this, user](bool success,
                    rest::Handle /*requestId*/,
                    rest::ServerConnection::ErrorOrEmpty /*result*/)
                {
                    if (success)
                        return;

                    resourcePool()->removeResources({user});
                });

            nx::vms::api::UserModelV3 body;
            body.id = user->getId();
            body.name = user->getName();
            body.email = user->getEmail();
            body.fullName = user->fullName();
            body.permissions = user->getRawPermissions();
            body.isEnabled = user->isEnabled();

            switch (digestSupport)
            {
                case QnUserResource::DigestSupport::enable:
                    body.isHttpDigestEnabled = true;
                    break;
                case QnUserResource::DigestSupport::disable:
                    body.isHttpDigestEnabled = false;
                    break;
                default:
                    body.isHttpDigestEnabled =
                        user->getDigest() != nx::vms::api::UserData::kHttpIsDisabledStub;
            }

            if (const auto password = user->getPassword(); !password.isEmpty())
                body.password = password;

            body.groupIds = user->userRoleIds();

            const auto accessRights = systemContext->accessRightsManager()->ownResourceAccessMap(
                user->getId());

            body.resourceAccessRights = {accessRights.keyValueBegin(), accessRights.keyValueEnd()};

            const auto tokenHelper = helper
                ? helper
                : systemContext->restApiHelper()->getSessionTokenHelper();

            if (newUsersSet.contains(user->getId()))
            {
                NX_ASSERT(body.password.has_value());

                api->putRest(tokenHelper,
                    QString("/rest/v3/users/%1").arg(user->getId().toString()),
                    network::rest::Params{},
                    nx::reflect::json::serialize(body).c_str(),
                    replyProcessor,
                    thread());
            }
            else
            {
                api->patchRest(tokenHelper,
                    QString("/rest/v3/users/%1").arg(user->getId().toString()),
                    network::rest::Params{},
                    nx::reflect::json::serialize(body).c_str(),
                    replyProcessor,
                    thread());
            }
        }
    }
    else
    {
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
                    NX_DEBUG(this, "About to remove resources because of an error: %1", errorCode);
                    resourcePool()->removeResources(newUsers);
                }
            });

        const auto currentUserName = QString::fromStdString(connectionCredentials().username);
        if (auto password = passwordIfUpdateIsRequired(currentUserName, users, digestSupport))
        {
            replyProcessor =
                makeReplyProcessorUpdatePassword(systemContext, replyProcessor, *password);
        }

        connection->getUserManager(Qn::kSystemAccess)->save(apiUsers, replyProcessor, this);
    }
}

void ResourcesChangesManager::saveAccessRights(const QnResourceAccessSubject& subject,
    const nx::core::access::ResourceAccessMap& accessRights,
    AccessRightsCallbackFunction callback,
    SystemContext* systemContext,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    if (systemContext->restApiHelper()->restApiEnabled())
    {
        const auto api = connectedServerApi();
        if (!api)
        {
            callback(false);
            return;
        }

        const auto accessRightsManager = this->systemContext()->accessRightsManager();
        const auto backup = accessRightsManager->ownResourceAccessMap(subject.id());

        if (backup == accessRights)
        {
            callback(true);
            return;
        }

        // Layouts require special handling.
        const auto resourcePool = this->systemContext()->resourcePool();

        const auto layouts = resourcePool->getResourcesByIds(nx::utils::keyRange(accessRights))
            .filtered<LayoutResource>(
            [](const QnLayoutResourcePtr& layout)
            {
                return !layout->isFile()
                    && !layout->isShared()
                    && !layout->hasFlags(Qn::cross_system);
            });

        for (const auto& layout: layouts)
        {
            layout->setParentId(QnUuid());
            auto systemContext = SystemContext::fromResource(layout);
            if (!NX_ASSERT(systemContext))
                continue;

            systemContext->layoutSnapshotManager()->save(layout);
        }

        accessRightsManager->setOwnResourceAccessMap(subject.id(), accessRights);

        auto handler =
            [this, subject, accessRights, backup, callback](bool success,
                rest::Handle /*requestId*/,
                rest::ServerConnection::ErrorOrEmpty /*result*/)
            {
                if (!success)
                {
                    this->systemContext()->accessRightsManager()->setOwnResourceAccessMap(
                        subject.id(), backup);
                }
                callback(success);
            };

        if (subject.isUser())
        {
            nx::vms::api::UserModelV3 body;
            body.id = subject.id();

            const auto user = resourcePool->getResourceById<QnUserResource>(body.id);
            if (NX_ASSERT(user))
            {
                body.name = user->getName();
                body.email = user->getEmail();
                body.type = user->userType();
                body.fullName = user->fullName();
                body.permissions = user->getRawPermissions();
                body.isEnabled = user->isEnabled();
                // Set isHttpDigestEnabled to false (default) for Cloud users so the server can
                // ignore this value.
                body.isHttpDigestEnabled =
                    user->getDigest() != nx::vms::api::UserData::kHttpIsDisabledStub
                    && user->userType() != nx::vms::api::UserType::cloud;
                body.groupIds = user->userRoleIds();
            }
            body.resourceAccessRights =
                {{accessRights.constKeyValueBegin(), accessRights.constKeyValueEnd()}};

            const auto tokenHelper = helper
                ? helper
                : systemContext->restApiHelper()->getSessionTokenHelper();

            api->patchRest(tokenHelper,
                QString("/rest/v3/users/%1").arg(body.id.toString()),
                network::rest::Params{},
                nx::reflect::json::serialize(body).c_str(),
                handler,
                thread());
        }
        else
        {
            nx::vms::api::UserGroupModel body;
            body.id = subject.id();
            // These fields are not std::optional, so the PATCH request will always overwrite them.
            const auto group = userRolesManager()->userRole(body.id);
            body.name = group.name;
            body.description = group.description;
            body.parentGroupIds = group.parentGroupIds;
            body.permissions = group.permissions;
            body.resourceAccessRights =
                {{accessRights.constKeyValueBegin(), accessRights.constKeyValueEnd()}};

            const auto tokenHelper = helper
                ? helper
                : systemContext->restApiHelper()->getSessionTokenHelper();

            api->patchRest(tokenHelper,
                QString("/rest/v3/userGroups/%1").arg(body.id.toString()),
                network::rest::Params{},
                nx::reflect::json::serialize(body).c_str(),
                handler,
                thread());
        }
    }
    else
    {
        const auto connection = messageBusConnection();
        if (!connection)
        {
            callback(false);
            return;
        }

        const auto accessRightsManager = this->systemContext()->accessRightsManager();
        const auto backup = accessRightsManager->ownResourceAccessMap(subject.id());

        if (backup == accessRights)
        {
            callback(true);
            return;
        }

        // Layouts require special handling.
        const auto resourcePool = this->systemContext()->resourcePool();

        const auto layouts = resourcePool->getResourcesByIds(nx::utils::keyRange(accessRights))
            .filtered<LayoutResource>(
            [](const QnLayoutResourcePtr& layout)
            {
                return !layout->isFile()
                    && !layout->isShared()
                    && !layout->hasFlags(Qn::cross_system);
            });

        for (const auto& layout: layouts)
        {
            layout->setParentId(QnUuid());
            auto systemContext = SystemContext::fromResource(layout);
            if (!NX_ASSERT(systemContext))
                continue;

            systemContext->layoutSnapshotManager()->save(layout);
        }

        accessRightsManager->setOwnResourceAccessMap(subject.id(), accessRights);

        auto handler =
            [this, subject, accessRights, backup, callback](int /*reqID*/,
                ec2::ErrorCode errorCode)
            {
                const bool error = errorCode != ec2::ErrorCode::ok;
                if (error)
                {
                    this->systemContext()->accessRightsManager()->setOwnResourceAccessMap(
                        subject.id(), backup);
                }
                callback(!error);
            };

        vms::api::AccessRightsData accessRightsData;
        accessRightsData.userId = subject.id();
        accessRightsData.resourceRights.insert(
            accessRights.constKeyValueBegin(), accessRights.constKeyValueEnd());

        connection->getUserManager(Qn::kSystemAccess)
            ->setAccessRights(accessRightsData, makeReplyProcessor(this, handler), this);
    }
}

void ResourcesChangesManager::saveUserRole(const nx::vms::api::UserRoleData& role,
    SystemContext* systemContext,
    RoleCallbackFunction callback,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    if (const auto mock = mockImplementation())
    {
        const bool success = mock->saveGroup(role);
        if (success)
            userRolesManager()->addOrUpdateUserRole(role);
        callback(success, role);
        return;
    }

    if (systemContext->restApiHelper()->restApiEnabled())
    {
        auto api = connectedServerApi();
        if (!api)
            return;

        auto backup = userRolesManager()->userRole(role.id);
        userRolesManager()->addOrUpdateUserRole(role);

        const bool isNewGroup = backup.id.isNull();

        auto handler =
            [this, backup, role, callback](bool success,
                rest::Handle /*requestId*/,
                rest::ServerConnection::ErrorOrEmpty /*result*/)
            {
                if (!success)
                {
                    if (backup.id.isNull())
                        userRolesManager()->removeUserRole(role.id); //< New group was not added.
                    else
                        userRolesManager()->addOrUpdateUserRole(backup);
                }
                if (callback)
                    callback(success, role);
            };

        const auto accessRights = systemContext->accessRightsManager()->ownResourceAccessMap(
            role.id);

        nx::vms::api::UserGroupModel body;
        body.id = role.id;
        body.name = role.name;
        body.description = role.description;
        body.parentGroupIds = role.parentGroupIds;
        body.permissions = role.permissions;
        body.resourceAccessRights = {accessRights.keyValueBegin(), accessRights.keyValueEnd()};

        const auto tokenHelper = helper
            ? helper
            : systemContext->restApiHelper()->getSessionTokenHelper();

        body.type = role.type;

        if (isNewGroup)
        {
            api->postRest(tokenHelper,
                QString("/rest/v3/userGroups"),
                network::rest::Params{},
                nx::reflect::json::serialize(body).c_str(),
                handler,
                thread());
        }
        else
        {
            api->patchRest(tokenHelper,
                QString("/rest/v3/userGroups/%1").arg(body.id.toString()),
                network::rest::Params{},
                nx::reflect::json::serialize(body).c_str(),
                handler,
                thread());
        }
    }
    else
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
                        userRolesManager()->removeUserRole(role.id); //< New group was not added.
                    else
                        userRolesManager()->addOrUpdateUserRole(backup);
                }
                if (callback)
                    callback(success, role);
            };

        connection->getUserManager(Qn::kSystemAccess)
            ->saveUserRole(role, makeReplyProcessor(this, handler), this);
    }
}

void ResourcesChangesManager::removeUserRole(const QnUuid& id,
    SystemContext* systemContext,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    if (systemContext->restApiHelper()->restApiEnabled())
    {
        auto api = connectedServerApi();
        if (!api)
            return;

        auto backup = userRolesManager()->userRole(id);
        userRolesManager()->removeUserRole(id);

        auto handler =
            [this, backup](bool success,
                rest::Handle /*requestId*/,
                rest::ServerConnection::ErrorOrEmpty /*result*/)
            {
                if (success)
                    return;

                userRolesManager()->addOrUpdateUserRole(backup);
            };

        const auto tokenHelper = helper
            ? helper
            : systemContext->restApiHelper()->getSessionTokenHelper();

        api->deleteRest(tokenHelper,
            QString("/rest/v3/userGroups/%1").arg(id.toString()),
            network::rest::Params{},
            handler,
            thread());
    }
    else
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

        connection->getUserManager(Qn::kSystemAccess)
            ->removeUserRole(id, makeReplyProcessor(this, handler), this);
    }
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

    auto replyProcessor =
        makeSaveResourceReplyProcessor<QnVideoWallResource, nx::vms::api::VideowallData>(
            this, videoWall, callback);

    if (applyChanges)
        applyChanges(videoWall);
    nx::vms::api::VideowallData apiVideowall;
    ec2::fromResourceToApi(videoWall, apiVideowall);

    connection->getVideowallManager(Qn::kSystemAccess)->save(apiVideowall, replyProcessor, this);
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

    auto replyProcessor =
        makeSaveResourceReplyProcessor<QnWebPageResource, nx::vms::api::WebPageData>(
            this, webPage, callback);

    if (applyChanges)
        applyChanges(webPage);
    nx::vms::api::WebPageData apiWebpage;
    ec2::fromResourceToApi(webPage, apiWebpage);

    connection->getWebPageManager(Qn::kSystemAccess)->save(apiWebpage, replyProcessor, this);

    // TODO: #sivanov Properties are not rolled back in case of failure.
    resourcePropertyDictionary()->saveParamsAsync({webPage->getId()});
}

} // namespace nx::vms::client::desktop
