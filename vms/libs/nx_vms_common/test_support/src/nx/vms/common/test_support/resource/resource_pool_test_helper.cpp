// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_test_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/user_role_data.h>

#include "storage_resource_stub.h"

QnUserResourcePtr QnResourcePoolTestHelper::createUser(GlobalPermissions globalPermissions,
    const QString& name,
    nx::vms::api::UserType userType,
    const QString& ldapDn)
{
    QnUserResourcePtr user(new QnUserResource(userType, ldapDn));
    user->setIdUnsafe(QnUuid::createUuid());
    user->setName(name);
    user->setPasswordAndGenerateHash(name);
    if (globalPermissions.testFlag(GlobalPermission::owner))
        user->setOwner(true);
    else
        user->setRawPermissions(globalPermissions);
    user->addFlags(Qn::remote);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addUser(GlobalPermissions globalPermissions,
    const QString& name,
    nx::vms::api::UserType userType,
    const QString& ldapDn)
{
    auto user = createUser(globalPermissions, name, userType, ldapDn);
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
    QnLayoutItemData item;
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
    camera->setTypeId(nx::vms::api::CameraData::kDesktopCameraTypeId);
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

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayoutForVideoWall(
    const QnVideoWallResourcePtr& videoWall)
{
    auto layout = createLayout();
    layout->setParentId(videoWall->getId());
    resourcePool()->addResource(layout);
    addVideoWallItem(videoWall, layout);
    return layout;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::addServer(nx::vms::api::ServerFlags additionalFlags)
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setUrl("http://localhost:7001");
    resourcePool()->addResource(server);
    server->setStatus(nx::vms::api::ResourceStatus::online);
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

nx::vms::api::UserRoleData QnResourcePoolTestHelper::createRole(
    GlobalPermissions permissions, std::vector<QnUuid> parentRoleIds)
{
    return createRole(NX_FMT("Role for %1", nx::reflect::json::serialize(permissions)),
        permissions, parentRoleIds);
}

nx::vms::api::UserRoleData QnResourcePoolTestHelper::createRole(const QString& name,
    GlobalPermissions permissions, std::vector<QnUuid> parentRoleIds)
{
    nx::vms::api::UserRoleData role{QnUuid::createUuid(), name, permissions, parentRoleIds};
    userRolesManager()->addOrUpdateUserRole(role);
    return role;
}

void QnResourcePoolTestHelper::removeRole(const QnUuid& roleId)
{
    userRolesManager()->removeUserRole(roleId);
}

void QnResourcePoolTestHelper::clear()
{
    resourcePool()->clear();

    const QSignalBlocker blocker(userRolesManager());
    userRolesManager()->resetCustomUserRoles({});
}
