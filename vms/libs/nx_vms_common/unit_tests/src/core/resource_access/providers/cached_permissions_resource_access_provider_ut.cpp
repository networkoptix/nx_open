// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/providers/cached_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/resource/storage_resource_stub.h>

namespace {

static const auto kSource = nx::core::access::Source::permissions;

} // namespace

namespace nx::core::access {
namespace test {

class CachedPermissionsResourceAccessProviderTest: public CachedBaseAccessProviderTestFixture
{
protected:
    void expectAccessGranted(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value = true)
    {
        if (value)
            expectAccess(subject, resource, kSource);
        else
            expectAccess(subject, resource, Source::none);
    }

    virtual AbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new PermissionsResourceAccessProvider(
            Mode::cached,
            systemContext());
    }
};

TEST_F(CachedPermissionsResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkSource)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kSource);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addLayout();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addWebPage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addVideoWall();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addServer();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(GlobalPermission::admin);
    auto server = addServer();
    auto target = addStorage(server);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkDesktopCameraAccess)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName2);

    auto user = addUser(GlobalPermission::admin);
    auto user2 = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);

    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
    ASSERT_FALSE(accessProvider()->hasAccess(user2, camera));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = addWebPage();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkServerAccess)
{
    auto target = addServer();

    /* Servers are now controlled via the same 'All Media' flag. */
    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkStorageAccess)
{
    auto server = addServer();
    auto target = addStorage(server);

    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkLayoutAccess)
{
    auto target = addLayout();

    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkUserAccess)
{
    auto user = addUser(GlobalPermission::accessAllMedia);
    auto target = addUser(GlobalPermission::accessAllMedia, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, user));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkVideoWallAccess)
{
    auto target = addVideoWall();

    auto user = addUser(GlobalPermission::controlVideowall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkUserRoleChangeAccess)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto parentRole = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(parentRole);

    user->setUserRoleIds({parentRole.id});
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    auto inheritedRole = createRole(GlobalPermission::none);
    ASSERT_FALSE(accessProvider()->hasAccess(inheritedRole, target));

    user->setUserRoleIds({inheritedRole.id});
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    inheritedRole.parentRoleIds = {parentRole.id};
    userRolesManager()->addOrUpdateUserRole(inheritedRole);
    ASSERT_TRUE(accessProvider()->hasAccess(inheritedRole, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    inheritedRole.parentRoleIds = {};
    userRolesManager()->addOrUpdateUserRole(inheritedRole);
    ASSERT_FALSE(accessProvider()->hasAccess(inheritedRole, target));
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setEnabled(false);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, nonPoolResourceAccess)
{
    auto user = addUser(GlobalPermission::admin);
    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitNewCameraAccess)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = createCamera();
    resourcePool()->addResource(camera);
    expectAccessGranted(user, camera);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitRemovedCameraAccess)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();
    resourcePool()->removeResource(camera);
    expectAccessGranted(user, camera, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitNewUserAccess)
{
    auto camera = addCamera();
    auto user = createUser(GlobalPermission::admin);
    resourcePool()->addResource(user);
    expectAccessGranted(user, camera);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitRemovedUserAccess)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    resourcePool()->removeResource(user);
    expectAccessGranted(user, camera, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitPermissionsChangedAccess)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::none);
    user->setRawPermissions(GlobalPermission::admin);
    expectAccessGranted(user, camera);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, suppressDuplicatedSignals)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    expectAccessGranted(user, camera, true);

    /* Here we should NOT receive the signal */
    user->setRawPermissions(GlobalPermission::accessAllMedia);
    expectAccessGranted(user, camera, true);

    /* Here we should receive the 'false' value. */
    resourcePool()->removeResource(user);
    expectAccessGranted(user, camera, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitRoleAccess)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::accessAllMedia);
    expectAccessGranted(role, target, true);

    userRolesManager()->removeUserRole(role.id);
    expectAccessGranted(role, target, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitInheritedRoleAccess)
{
    auto target = addCamera();

    auto parentRole = createRole(GlobalPermission::accessAllMedia);
    expectAccessGranted(parentRole, target, true);

    auto inheritedRole = createRole(GlobalPermission::none);
    expectAccessGranted(inheritedRole, target, false);

    inheritedRole.parentRoleIds = {parentRole.id};
    userRolesManager()->addOrUpdateUserRole(inheritedRole);
    expectAccessGranted(inheritedRole, target, true);

    userRolesManager()->removeUserRole(parentRole.id);
    expectAccessGranted(parentRole, target, false);
    expectAccessGranted(inheritedRole, target, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, suppressDuplicatedRoleSignals)
{
    auto target = addCamera();
    auto role = createRole(GlobalPermission::accessAllMedia);
    expectAccessGranted(role, target, true);

    /* Here we should NOT receive the signal */
    role.permissions = GlobalPermission::accessAllMedia | GlobalPermission::exportArchive;
    userRolesManager()->addOrUpdateUserRole(role);
    expectAccessGranted(role, target, true);

    /* Here we should receive the 'false' value. */
    userRolesManager()->removeUserRole(role.id);
    expectAccessGranted(role, target, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitRoleAccessToNewCameras)
{
    auto role = createRole(GlobalPermission::accessAllMedia);

    auto target = createCamera();
    resourcePool()->addResource(target);
    expectAccessGranted(role, target, true);

    resourcePool()->removeResource(target);
    expectAccessGranted(role, target, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitInheritedRoleAccessToNewCameras)
{
    auto parentRole = createRole(GlobalPermission::accessAllMedia);

    auto target = createCamera();
    resourcePool()->addResource(target);
    expectAccessGranted(parentRole, target, true);

    auto inheritedRole = createRole(GlobalPermission::none, {parentRole.id});
    expectAccessGranted(inheritedRole, target, true);

    resourcePool()->removeResource(target);
    expectAccessGranted(parentRole, target, false);
    expectAccessGranted(inheritedRole, target, false);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitUserAccessChangeOnRolePermissionsChange)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::none);

    auto user = addUser(GlobalPermission::none);
    user->setUserRoleIds({role.id});
    expectAccessGranted(user, target, false);

    role.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(role);
    expectAccessGranted(user, target, true);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, awaitUserAccessChangeOnInheritedRolePermissionsChange)
{
    auto target = addCamera();

    auto parentRole = createRole(GlobalPermission::none);
    auto inheritedRole = createRole(GlobalPermission::none, {parentRole.id});

    auto user = addUser(GlobalPermission::none);
    user->setUserRoleIds({inheritedRole.id});
    expectAccessGranted(user, target, false);

    parentRole.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(parentRole);
    expectAccessGranted(user, target, true);
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkAccessToOwnLayout)
{
    auto target = createLayout();
    auto user = addUser(GlobalPermission::none);
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, checkParentIdChange)
{
    /* We cannot test parent id change from null to user's as it never happens
     * (and therefore no signal is sent from QnResource in this case). */
    auto user = addUser(GlobalPermission::none);
    auto target = createLayout();
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    target->setParentId(QnUuid());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedPermissionsResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}

} // namespace test
} // namespace nx::core::access
