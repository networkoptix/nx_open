// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_test_helper.h"

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "storage_resource_stub.h"

using namespace nx::vms::api;

QnUserResourcePtr QnResourcePoolTestHelper::createUser(
    Ids parentGroupIds,
    const QString& name,
    UserType userType,
    GlobalPermissions globalPermissions,
    const std::map<QnUuid, nx::vms::api::AccessRights>& resourceAccessRights,
    const QString& ldapDn)
{
    QnUserResourcePtr user(new QnUserResource(userType, {ldapDn}));
    user->setIdUnsafe(QnUuid::createUuid());
    user->setName(name);
    user->setPasswordAndGenerateHash(name);
    user->setGroupIds(parentGroupIds.data);
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
    const std::map<QnUuid, nx::vms::api::AccessRights>& resourceAccessRights,
    const QString& ldapDn)
{
    const auto user = createUser(
        parentGroupIds, name, userType, globalPermissions, resourceAccessRights, ldapDn);
    resourcePool()->addResource(user);
    return user;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::createLayout()
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid());
    return layout;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayout()
{
    auto layout = createLayout();
    resourcePool()->addResource(layout);
    return layout;
}

QnUuid QnResourcePoolTestHelper::addToLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourcePtr& resource)
{
    nx::vms::common::LayoutItemData item;
    item.uuid = QnUuid::createUuid();
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
        QnUuid::createUuid().toString() //< Running client instance id must be here.
        + user->getId().toString());
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addDesktopCamera(
    const QnUserResourcePtr& user)
{
    auto camera = createDesktopCamera(user);
    resourcePool()->addResource(camera);
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addIntercomCamera()
{
    const auto camera = createCamera();

    static const QString kOpenDoorPortName = QString::fromStdString(
        nx::reflect::toString(ExtendedCameraOutput::powerRelay));

    QnIOPortData intercomFeaturePort;
    intercomFeaturePort.outputName = kOpenDoorPortName;

    camera->setIoPortDescriptions({intercomFeaturePort}, false);
    resourcePool()->addResource(camera);
    return camera;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addIntercomLayout(
    const QnVirtualCameraResourcePtr& intercomCamera)
{
    if (!NX_ASSERT(intercomCamera && nx::vms::common::isIntercom(intercomCamera)))
        return {};

    const auto intercomId = intercomCamera->getId();

    const auto layout = createLayout();
    layout->setIdUnsafe(nx::vms::common::calculateIntercomLayoutId(intercomId));
    layout->setParentId(intercomId);

    resourcePool()->addResource(layout);
    NX_ASSERT(nx::vms::common::isIntercomLayout(layout));
    return layout;
}

QnWebPageResourcePtr QnResourcePoolTestHelper::addWebPage()
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setIdUnsafe(QnUuid::createUuid());
    webPage->setUrl("http://www.ru");
    webPage->setName(QnWebPageResource::nameForUrl(webPage->getUrl()));
    resourcePool()->addResource(webPage);
    return webPage;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::createVideoWall()
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setIdUnsafe(QnUuid::createUuid());
    return videoWall;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::addVideoWall()
{
    auto videoWall = createVideoWall();
    resourcePool()->addResource(videoWall);
    return videoWall;
}

QnUuid QnResourcePoolTestHelper::addVideoWallItem(const QnVideoWallResourcePtr& videoWall,
    const QnLayoutResourcePtr& itemLayout)
{
    QnVideoWallItem vwitem;
    vwitem.uuid = QnUuid::createUuid();

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
    const QnUuid& itemId,
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

QnMediaServerResourcePtr QnResourcePoolTestHelper::addServer(ServerFlags additionalFlags)
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setUrl("http://localhost:7001");
    resourcePool()->addResource(server);
    server->setStatus(ResourceStatus::online);
    server->setServerFlags(server->getServerFlags() | additionalFlags);
    return server;
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
    const std::map<QnUuid, nx::vms::api::AccessRights>& resourceAccessRights,
    GlobalPermissions permissions)
{
    UserGroupData group{QnUuid::createUuid(), name, permissions, parentGroupIds.data};
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

void QnResourcePoolTestHelper::removeUserGroup(const QnUuid& groupId)
{
    systemContext()->userGroupManager()->remove(groupId);
}

void QnResourcePoolTestHelper::clear()
{
    resourcePool()->clear();
    systemContext()->userGroupManager()->resetAll({});
}
