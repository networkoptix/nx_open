// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_test_helper.h"

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/storage_resource_stub.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

using namespace nx::vms::api;

QnResourcePoolTestHelper::QnResourcePoolTestHelper(nx::vms::common::SystemContext* context):
    SystemContextAware(context)
{
}

QnResourcePoolTestHelper::~QnResourcePoolTestHelper()
{
}

QnUserResourcePtr QnResourcePoolTestHelper::createUser(
    Ids parentGroupIds,
    const QString& name,
    UserType userType,
    GlobalPermissions globalPermissions,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights,
    const QString& ldapDn)
{
    QnUserResourcePtr user(new QnUserResource(userType, {ldapDn}));
    user->setIdUnsafe(nx::Uuid::createUuid());
    user->setName(name);
    user->setPasswordAndGenerateHash(name);
    user->setSiteGroupIds(parentGroupIds.data);
    user->setRawPermissions(globalPermissions);
    user->setResourceAccessRights(resourceAccessRights);
    user->addFlags(Qn::remote);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addUser(
    Ids parentGroupIds,
    const QString& name,
    UserType userType,
    GlobalPermissions globalPermissions,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights,
    const QString& ldapDn)
{
    const auto user = createUser(
        parentGroupIds, name, userType, globalPermissions, resourceAccessRights, ldapDn);
    resourcePool()->addResource(user);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addUser(
    const QString& name, const std::vector<nx::Uuid>& groupIds) const
{
    QnUserResourcePtr user(new QnUserResource(nx::vms::api::UserType::local, /*externalId*/ {}));
    user->setIdUnsafe(nx::Uuid::createUuid());
    user->setName(name);
    user->setSiteGroupIds(groupIds);
    user->addFlags(Qn::remote);
    resourcePool()->addResource(user);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addOrgUser(
    Ids orgGroupIds,
    const QString& name,
    GlobalPermissions globalPermissions,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights)
{
    auto user = createUser(
        NoGroup, name, nx::vms::api::UserType::cloud, globalPermissions, resourceAccessRights);
    user->setOrgGroupIds(orgGroupIds.data);
    resourcePool()->addResource(user);
    return user;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::createServer(const nx::Uuid& id) const
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(id);
    return server;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::addServer(
    const QString& name,
    const nx::Uuid& id) const
{
    QnMediaServerResourcePtr server = createServer(id);
    server->setName(name);
    resourcePool()->addResource(server);
    return server;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::addEdgeServer(const QString& name, const QString& address) const
{
    QnMediaServerResourcePtr server = createServer();
    server->setName(name);
    server->setServerFlags(server->getServerFlags() | ServerFlag::SF_Edge);
    server->setNetAddrList({nx::network::SocketAddress(address.toStdString())});
    resourcePool()->addResource(server);
    return server;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayout(
    const QString& name,
    const nx::Uuid& parentId) const
{
    QnLayoutResourcePtr layout = createLayout();
    layout->setName(name);
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->setParentId(parentId);
    resourcePool()->addResource(layout);
    return layout;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::createLayout() const
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->setName(nx::Uuid::createUuid().toSimpleString());
    return layout;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayout()
{
    auto layout = createLayout();
    resourcePool()->addResource(layout);
    return layout;
}

nx::Uuid QnResourcePoolTestHelper::addToLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourcePtr& resource)
{
    nx::vms::common::LayoutItemData item;
    item.uuid = nx::Uuid::createUuid();
    item.resource.id = resource->getId();
    layout->addItem(item);
    return item.uuid;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::createCamera(Qn::LicenseType licenseType)
{
    nx::CameraResourceStubPtr camera(new nx::CameraResourceStub(licenseType));
    camera->setName("camera");
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addCamera(Qn::LicenseType licenseType)
{
    auto camera = createCamera(licenseType);
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addCamera(
    const QString& name,
    const nx::Uuid& parentId,
    const QString& hostAddress) const
{
    const auto camera = createCamera();
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(parentId);
    if (!hostAddress.isEmpty())
        camera->setHostAddress(hostAddress);
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addEdgeCamera(
    const QString& name, const QnMediaServerResourcePtr& edgeServer) const
{
    const auto camera = createCamera();
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(edgeServer->getId());
    if (NX_ASSERT(!edgeServer->getNetAddrList().empty(), "Edge server should have some network address"))
        camera->setHostAddress(nx::toQString(edgeServer->getNetAddrList().first().address.toString()));
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addVirtualCamera(
    const QString& name,
    const nx::Uuid& parentId /*= nx::Uuid()*/) const
{
    const auto camera = createCamera();
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(parentId);
    camera->setFlags(camera->flags() | Qn::ResourceFlag::virtual_camera);
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addIOModule(
    const QString& name,
    const nx::Uuid& parentId /*= nx::Uuid()*/) const
{
    const auto camera = createCamera();
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(parentId);
    camera->setFlags(camera->flags() | Qn::ResourceFlag::io_module);
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addRecorderCamera(
    const QString& name,
    const QString& groupId,
    const nx::Uuid& parentId /*= nx::Uuid()*/) const
{
    const auto camera = createCamera();
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(parentId);
    camera->setGroupId(groupId);
    camera->markCameraAsNvr();
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addMultisensorSubCamera(
    const QString& name,
    const QString& groupId,
    const nx::Uuid& parentId /*= nx::Uuid()*/) const
{
    const auto camera = createCamera();
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(parentId);
    camera->setGroupId(groupId);
    resourcePool()->addResource(camera);
    return camera;
}

std::vector<nx::CameraResourceStubPtr> QnResourcePoolTestHelper::addCameras(size_t count)
{
    std::vector<nx::CameraResourceStubPtr> cameras;
    cameras.reserve(count);
    for (size_t i = 0; i < count; ++i)
        cameras.push_back(addCamera());
    return cameras;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::createDesktopCamera(
    const QnUserResourcePtr& user)
{
    auto camera = createCamera(Qn::LicenseType::LC_Invalid);
    camera->setTypeId(CameraData::kDesktopCameraTypeId);
    camera->setFlags(Qn::desktop_camera);
    camera->setModel("virtual desktop camera"); //< TODO: #sivanov Globalize the constant.
    camera->setName(user->getName());
    camera->setPhysicalId(
        nx::Uuid::createUuid().toString(QUuid::WithBraces) //< Running client instance id must be here.
        + user->getId().toString(QUuid::WithBraces));
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addDesktopCamera(
    const QnUserResourcePtr& user)
{
    auto camera = createDesktopCamera(user);
    resourcePool()->addResource(camera);
    return camera;
}

void QnResourcePoolTestHelper::toIntercom(nx::CameraResourceStubPtr camera)
{
    QnIOPortData intercomFeaturePort;
    intercomFeaturePort.outputName = QnVirtualCameraResource::intercomSpecificPortName();

    camera->setProperty(
        nx::vms::api::device_properties::kIoSettings,
        QString::fromStdString(nx::reflect::json::serialize(
            QnIOPortDataList{intercomFeaturePort})));
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addIntercomCamera()
{
    const auto camera = createCamera();
    toIntercom(camera);
    resourcePool()->addResource(camera);
    return camera;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addIntercomLayout(
    const QnVirtualCameraResourcePtr& intercomCamera)
{
    if (!NX_ASSERT(intercomCamera))
        return {};

    const auto intercomId = intercomCamera->getId();

    const auto layout = createLayout();
    layout->setIdUnsafe(nx::vms::common::calculateIntercomLayoutId(intercomId));
    layout->setParentId(intercomId);

    resourcePool()->addResource(layout);
    NX_ASSERT(!intercomCamera->isIntercom()
        || (intercomCamera->isIntercom() && nx::vms::common::isIntercomLayout(layout)));
    return layout;
}

QnWebPageResourcePtr QnResourcePoolTestHelper::addWebPage()
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setIdUnsafe(nx::Uuid::createUuid());
    webPage->setUrl("http://www.ru");
    webPage->setName(QnWebPageResource::nameForUrl(webPage->getUrl()));
    resourcePool()->addResource(webPage);
    return webPage;
}

void QnResourcePoolTestHelper::addToLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& resources) const
{
    for (const auto& resource: resources)
    {
        nx::vms::common::LayoutItemData layoutItemData;
        layoutItemData.resource.id = resource->getId();
        layoutItemData.uuid = nx::Uuid::createUuid();
        layout->addItem(layoutItemData);
    }
}

void QnResourcePoolTestHelper::setVideoWallScreenRuntimeStatus(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& videoWallScreen,
    bool isOnline,
    const nx::Uuid& controlledBy)
{
    QnVideoWallItem updatedVideoWallScreen = videoWallScreen;
    updatedVideoWallScreen.runtimeStatus.online = isOnline;
    updatedVideoWallScreen.runtimeStatus.controlledBy = controlledBy;
    videoWall->items()->updateItem(updatedVideoWallScreen);
}

void QnResourcePoolTestHelper::setupAccessToResourceForUser(
    const QnUserResourcePtr& user,
    const QnResourcePtr& resource,
    bool isAccessible) const
{
    setupAccessToResourceForUser(user, resource, isAccessible
        ? kViewAccessRights
        : kNoAccessRights);
}

void QnResourcePoolTestHelper::setupAccessToResourceForUser(
    const QnUserResourcePtr& user,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRights accessRights) const
{
    if (NX_ASSERT(resource))
        setupAccessToObjectForUser(user, resource->getId(), accessRights);
}

void QnResourcePoolTestHelper::setupAccessToObjectForUser(
    const QnUserResourcePtr& user,
    const nx::Uuid& resourceOrGroupId,
    nx::vms::api::AccessRights accessRights) const
{
    if (!NX_ASSERT(user && !resourceAccessManager()->hasPowerUserPermissions(user)))
        return;

    const auto userId = user->getId();
    const auto accessRightsManager = systemContext()->accessRightsManager();

    auto accessRightsMap = accessRightsManager->ownResourceAccessMap(userId);
    if (accessRights != 0)
        accessRightsMap.emplace(resourceOrGroupId, accessRights);
    else
        accessRightsMap.remove(resourceOrGroupId);

    accessRightsManager->setOwnResourceAccessMap(userId, accessRightsMap);
}

void QnResourcePoolTestHelper::setupAllMediaAccess(
    const QnUserResourcePtr& user, AccessRights accessRights) const
{
    setupAccessToObjectForUser(user, kAllDevicesGroupId, accessRights);
    setupAccessToObjectForUser(user, kAllWebPagesGroupId, accessRights & AccessRight::view);
    setupAccessToObjectForUser(user, kAllServersGroupId, accessRights & AccessRight::view);
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::createVideoWall()
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setIdUnsafe(nx::Uuid::createUuid());
    return videoWall;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::addVideoWall()
{
    auto videoWall = createVideoWall();
    resourcePool()->addResource(videoWall);
    return videoWall;
}

nx::Uuid QnResourcePoolTestHelper::addVideoWallItem(const QnVideoWallResourcePtr& videoWall,
    const QnLayoutResourcePtr& itemLayout)
{
    QnVideoWallItem vwitem;
    vwitem.uuid = nx::Uuid::createUuid();

    if (itemLayout)
    {
        itemLayout->setParentId(videoWall->getId());
        vwitem.layout = itemLayout->getId();
    }

    videoWall->items()->addItem(vwitem);
    return vwitem.uuid;
}

bool QnResourcePoolTestHelper::changeVideoWallItem(
    const QnVideoWallResourcePtr& videoWall,
    const nx::Uuid& itemId,
    const QnLayoutResourcePtr& itemLayout)
{
    auto vwitem = videoWall->items()->getItem(itemId);
    if (vwitem.uuid.isNull())
        return false;

    if (itemLayout)
    {
        itemLayout->setParentId(videoWall->getId());
        vwitem.layout = itemLayout->getId();
    }
    else
    {
        vwitem.layout = {};
    }

    videoWall->items()->addOrUpdateItem(vwitem);
    return true;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayoutForVideoWall(
    const QnVideoWallResourcePtr& videoWall)
{
    auto layout = createLayout();
    layout->setParentId(videoWall->getId());
    resourcePool()->addResource(layout);
    addVideoWallItem(videoWall, layout);
    return layout;
}

QnStorageResourcePtr QnResourcePoolTestHelper::addStorage(const QnMediaServerResourcePtr& server)
{
    QnStorageResourcePtr storage(new nx::StorageResourceStub());
    storage->setParentId(server->getId());
    resourcePool()->addResource(storage);
    return storage;
}

UserGroupData QnResourcePoolTestHelper::createUserGroup(
    QString name,
    Ids parentGroupIds,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights,
    GlobalPermissions permissions)
{
    UserGroupData group{nx::Uuid::createUuid(), name, permissions, parentGroupIds.data};
    group.resourceAccessRights = resourceAccessRights;
    addOrUpdateUserGroup(group);
    return group;
}

void QnResourcePoolTestHelper::addOrUpdateUserGroup(const UserGroupData& group)
{
    systemContext()->userGroupManager()->addOrUpdate(group);
    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        group.id, {group.resourceAccessRights.begin(), group.resourceAccessRights.end()});
}

void QnResourcePoolTestHelper::removeUserGroup(const nx::Uuid& groupId)
{
    systemContext()->userGroupManager()->remove(groupId);
}

void QnResourcePoolTestHelper::clear()
{
    using namespace nx::vms::common;

    systemContext()->accessRightsManager()->resetAccessRights({});
    resourcePool()->clear(/*notify*/ true);
    systemContext()->showreelManager()->resetShowreels();

    // We cannot use `resetAll` method as `NonEditableUsersAndGroups` is connected to the `reset`
    // signal via queued connection. Thus groups are not actually removed from cache.
    // systemContext()->userGroupManager()->resetAll({});
    auto groups = systemContext()->userGroupManager()->ids(UserGroupManager::Selection::custom);
    for (auto groupId: groups)
        systemContext()->userGroupManager()->remove(groupId);
}

nx::vms::common::AnalyticsPluginResourcePtr QnResourcePoolTestHelper::addAnalyticsIntegration(
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    using namespace nx::vms::common;

    AnalyticsPluginResourcePtr integration(new AnalyticsPluginResource());
    integration->setIdUnsafe(nx::Uuid::createUuid());
    integration->setTypeId(nx::vms::api::AnalyticsPluginData::kResourceTypeId);
    resourcePool()->addResource(integration);

    auto engine = AnalyticsEngineResourcePtr(new AnalyticsEngineResource());
    engine->setIdUnsafe(nx::Uuid::createUuid());
    engine->setTypeId(nx::vms::api::AnalyticsEngineData::kResourceTypeId);
    engine->setParentId(integration->getId());
    resourcePool()->addResource(engine);

    engine->setManifest(manifest);

    return integration;
}
