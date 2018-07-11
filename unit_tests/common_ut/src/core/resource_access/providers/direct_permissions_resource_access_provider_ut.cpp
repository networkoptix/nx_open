#include <core/resource_access/providers/direct_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <test_support/resource/storage_resource_stub.h>
#include <test_support/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

#include <common/common_module.h>

using namespace nx::core::access;

namespace {

static const auto kSource = Source::permissions;

}

class QnDirectPermissionsResourceAccessProviderTest: public QnDirectBaseAccessProviderTestFixture
{
protected:
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnPermissionsResourceAccessProvider(Mode::direct, commonModule());
    }
};

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkSource)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kSource);
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addLayout();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addWebPage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addVideoWall();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addServer();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(GlobalPermission::admin);
    auto server = addServer();
    auto target = addStorage(server);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByName)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName2);

    auto user = addUser(GlobalPermission::admin);
    auto user2 = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);

    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
    ASSERT_TRUE(accessProvider()->hasAccess(user2, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByPermissions)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName);

    auto user = addUser(GlobalPermission::controlVideowall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = addWebPage();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkServerAccess)
{
    auto target = addServer();

    /* Servers are now controlled via the same 'All Media' flag. */
    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::editCameras);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkStorageAccess)
{
    auto server = addServer();
    auto target = addStorage(server);

    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkLayoutAccess)
{
    auto target = addLayout();

    auto user = addUser(GlobalPermission::admin);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkUserAccess)
{
    auto user = addUser(GlobalPermission::accessAllMedia);
    auto target = addUser(GlobalPermission::accessAllMedia, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkVideoWallAccess)
{
    auto target = addVideoWall();

    auto user = addUser(GlobalPermission::controlVideowall);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkUserRoleChangeAccess)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setEnabled(false);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, nonPoolResourceAccess)
{
    auto user = addUser(GlobalPermission::admin);
    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkRoleCameraAccess)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkRolePermissionsChange)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_FALSE(accessProvider()->hasAccess(role, target));

    role.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAccessToOwnLayout)
{
    auto target = createLayout();
    auto user = addUser(GlobalPermission::none);
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkParentIdChange)
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

TEST_F(QnDirectPermissionsResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}
