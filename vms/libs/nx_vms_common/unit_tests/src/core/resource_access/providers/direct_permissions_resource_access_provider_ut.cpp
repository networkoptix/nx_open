// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/providers/direct_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/resource/storage_resource_stub.h>

using namespace nx::core::access;

namespace {

static const auto kSource = Source::permissions;

} // namespace

namespace nx::core::access {
namespace test {

class DirectPermissionsResourceAccessProviderTest: public DirectBaseAccessProviderTestFixture
{
protected:
    virtual AbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new PermissionsResourceAccessProvider(Mode::direct, commonModule());
    }
};

TEST_F(DirectPermissionsResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkSource)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kSource);
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addLayout();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addWebPage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addVideoWall();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addServer();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(GlobalPermission::admin);
    auto server = addServer();
    auto target = addStorage(server);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkDesktopCameraAccess)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName2);

    auto user = addUser(GlobalPermission::admin);
    auto user2 = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);

    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
    ASSERT_FALSE(accessProvider()->hasAccess(user2, camera));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = addWebPage();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkServerAccess)
{
    auto target = addServer();

    /* Servers are now controlled via the same 'All Media' flag. */
    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkStorageAccess)
{
    auto server = addServer();
    auto target = addStorage(server);

    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkLayoutAccess)
{
    auto target = addLayout();

    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkUserAccess)
{
    auto user = addUser(GlobalPermission::accessAllMedia);
    auto target = addUser(GlobalPermission::accessAllMedia, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, user));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkVideoWallAccess)
{
    auto target = addVideoWall();

    auto user = addUser(GlobalPermission::controlVideowall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkUserRoleChangeAccess)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setEnabled(false);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, nonPoolResourceAccess)
{
    auto user = addUser(GlobalPermission::admin);
    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkRoleCameraAccess)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkRolePermissionsChange)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_FALSE(accessProvider()->hasAccess(role, target));

    role.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkAccessToOwnLayout)
{
    auto target = createLayout();
    auto user = addUser(GlobalPermission::none);
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectPermissionsResourceAccessProviderTest, checkParentIdChange)
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

TEST_F(DirectPermissionsResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}

} // namespace test
} // namespace nx::core::access
