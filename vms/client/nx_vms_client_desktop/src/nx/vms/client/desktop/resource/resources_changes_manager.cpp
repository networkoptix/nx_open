// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_changes_manager.h"

#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/reflect/json/serializer.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/api/data/server_model.h>
#include <nx/vms/api/data/user_group_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>
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

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

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
{
}

ResourcesChangesManager::~ResourcesChangesManager()
{
}

void ResourcesChangesManager::deleteResource(const QnResourcePtr& resource,
    const DeleteResourceCallbackFunction& callback,
    const nx::vms::common::SessionTokenHelperPtr& helper)
{
    auto systemContext = SystemContext::fromResource(resource);
    if (!NX_ASSERT(systemContext))
        return;

    auto api = systemContext->connectedServerApi();
    if (!api)
        return;

    auto handler =
        [this, callback, resource](bool success,
            rest::Handle /*requestId*/,
            rest::ServerConnection::ErrorOrEmpty result)
        {
            if (success)
            {
                NX_DEBUG(this, "About to remove resource %1", resource);
                auto systemContext = SystemContext::fromResource(resource);
                if (NX_ASSERT(systemContext))
                    systemContext->resourcePool()->removeResource(resource);
            }

            nx::network::rest::Result::Error errorCode = success
                ? nx::network::rest::Result::NoError
                : nx::network::rest::Result::InternalServerError;

            if (auto error = std::get_if<nx::network::rest::Result>(&result))
                errorCode = error->error;

            if (callback)
                callback(success, resource, errorCode);

            if (!success && errorCode != nx::network::rest::Result::SessionExpired)
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
        action = QString("/rest/v3/videoWalls/%1").arg(resource->getId().toString());
    }
    else if (resource.dynamicCast<QnWebPageResource>())
    {
        action = QString("/rest/v3/webPages/%1").arg(resource->getId().toString());
    }

    const auto tokenHelper = helper
        ? helper
        : systemContext->restApiHelper()->getSessionTokenHelper();

    api->deleteRest(tokenHelper, action, network::rest::Params{}, handler, this);
}

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

    auto systemContext = SystemContext::fromResource(cameras.first());
    if (!NX_ASSERT(systemContext))
        return;

    NX_ASSERT(std::all_of(cameras.cbegin(), cameras.cend(),
        [systemContext](const auto& camera)
        {
            return SystemContext::fromResource(camera) == systemContext;
        }), "Implement saving cameras from different systems");

    auto connection = systemContext->messageBusConnection();
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

    connection->getCameraManager(Qn::kSystemAccess)->saveUserAttributes(changes, handler, this);

    // TODO: #sivanov Values are not rolled back in case of failure.
    auto idList = idListFromResList(cameras);
    systemContext->resourcePropertyDictionary()->saveParamsAsync(idList);
}

void ResourcesChangesManager::saveCamerasCore(
    const QnVirtualCameraResourceList& cameras, CameraChangesFunction applyChanges)
{
    NX_ASSERT(applyChanges); //< ::saveCamerasCore is to be removed someday.
    if (!applyChanges)
        return;

    if (cameras.isEmpty())
        return;

    auto systemContext = SystemContext::fromResource(cameras.first());
    if (!NX_ASSERT(systemContext))
        return;

    NX_ASSERT(std::all_of(cameras.cbegin(), cameras.cend(),
        [systemContext](const auto& camera)
        {
            return SystemContext::fromResource(camera) == systemContext;
        }), "Implement saving cameras from different systems");

    auto connection = systemContext->messageBusConnection();
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

            auto systemContext = SystemContext::fromResource(cameras.first());
            if (NX_ASSERT(systemContext))
            {
                for (const auto& data: backup)
                {
                    auto camera = systemContext->resourcePool()
                        ->getResourceById<QnVirtualCameraResource>(data.id);
                    if (camera)
                        ec2::fromApiToResource(data, camera);
                }
            }

            emit saveChangesFailed(cameras);
        };

    for (const auto& camera: cameras)
        applyChanges(camera);

    nx::vms::api::CameraDataList apiCameras;
    ec2::fromResourceListToApi(cameras, apiCameras);
    connection->getCameraManager(Qn::kSystemAccess)->addCameras(apiCameras, handler, this);
}

rest::Handle ResourcesChangesManager::saveServer(
    const QnMediaServerResourcePtr& server,
    SaveServerCallback callback)
{
    if (!NX_ASSERT(server))
        return {};

    const auto systemContext = SystemContext::fromResource(server);
    if (!NX_ASSERT(systemContext))
        return {};

    const auto api = systemContext->connectedServerApi();
    if (!api) //< Connection is broken already.
        return {};

    auto internalCallback =
        [this, server, callback=std::move(callback)](
            bool success,
            rest::Handle requestId,
            rest::ErrorOrData<QByteArray> reply)
        {
            if (success && std::holds_alternative<nx::network::rest::Result>(reply))
            {
                const auto& error = std::get<nx::network::rest::Result>(reply);
                NX_ERROR(this, "Save changes failed: %1", error.errorString);
                success = false;
            }

            if (!success)
                emit saveChangesFailed({{server}});

            if (callback)
                callback(success, requestId);
        };

    nx::vms::api::ServerModel body;

    auto change = server->userAttributes();

    body.id = change.serverId;
    body.backupBitrateBytesPerSecond = change.backupBitrateBytesPerSecond;
    body.name = change.serverName.isEmpty() ? server->getName() : change.serverName;
    body.isFailoverEnabled = change.allowAutoRedundancy;
    body.maxCameras = change.maxCameras;
    body.locationId = change.locationId;

    auto modifiedProperties = systemContext->resourcePropertyDictionary()
        ->modifiedProperties(server->getId());
    for (auto [key, value]: nx::utils::keyValueRange(modifiedProperties))
        body.parameters[key] = QJsonValue(value);

    return api->patchRest(systemContext->restApiHelper()->getSessionTokenHelper(),
        QString("/rest/v3/servers/%1").arg(server->getId().toString()),
        network::rest::Params{},
        QByteArray::fromStdString(nx::reflect::json::serialize(body)),
        internalCallback,
        this);
}

void ResourcesChangesManager::saveVideoWall(
    const QnVideoWallResourcePtr& videoWall,
    VideoWallCallbackFunction callback)
{
    NX_ASSERT(videoWall);
    if (!videoWall)
        return;

    const auto systemContext = SystemContext::fromResource(videoWall);
    if (!NX_ASSERT(systemContext))
        return;

    auto connection = systemContext->messageBusConnection();
    if (!connection)
        return;

    auto handler =
        [this, videoWall, callback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = (errorCode == ec2::ErrorCode::ok);

            if (callback)
                callback(success, videoWall);

            if (!success)
                emit saveChangesFailed({{videoWall}});
        };

    nx::vms::api::VideowallData apiVideowall;
    ec2::fromResourceToApi(videoWall, apiVideowall);

    connection->getVideowallManager(Qn::kSystemAccess)->save(apiVideowall, handler, this);
}

void ResourcesChangesManager::saveWebPage(
    const QnWebPageResourcePtr& webPage,
    WebPageCallbackFunction callback)
{
    if (!NX_ASSERT(webPage))
        return;

    const auto systemContext = SystemContext::fromResource(webPage);
    if (!NX_ASSERT(systemContext))
        return;

    auto connection = systemContext->messageBusConnection();
    if (!connection)
        return;

    auto handler =
        [this, webPage, callback](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            const bool success = (errorCode == ec2::ErrorCode::ok);
            const auto systemContext = SystemContext::fromResource(webPage);

            // Save resource properties only after resource itself is saved.
            if (success && NX_ASSERT(systemContext))
                systemContext->resourcePropertyDictionary()->saveParamsAsync({webPage->getId()});

            if (callback)
                callback(success, webPage);

            if (!success)
                emit saveChangesFailed({{webPage}});
        };

    nx::vms::api::WebPageData apiWebpage;
    ec2::fromResourceToApi(webPage, apiWebpage);
    connection->getWebPageManager(Qn::kSystemAccess)->save(apiWebpage, handler, this);
}

} // namespace nx::vms::client::desktop
