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

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkSource)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kSource);
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addLayout();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addWebPage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addVideoWall();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addServer();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto server = addServer();
    auto target = addStorage(server);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByName)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName2);

    auto user = addUser(Qn::GlobalAdminPermission);
    auto user2 = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);

    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
    ASSERT_TRUE(accessProvider()->hasAccess(user2, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByPermissions)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName);

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = addWebPage();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkServerAccess)
{
    auto target = addServer();

    /* Servers are now controlled via the same 'All Media' flag. */
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkStorageAccess)
{
    auto server = addServer();
    auto target = addStorage(server);

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkLayoutAccess)
{
    auto target = addLayout();

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkUserAccess)
{
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    auto target = addUser(Qn::GlobalAccessAllMediaPermission, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkVideoWallAccess)
{
    auto target = addVideoWall();

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkUserRoleChangeAccess)
{
    auto target = addCamera();

    auto user = addUser(Qn::NoGlobalPermissions);
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setEnabled(false);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, nonPoolResourceAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkRoleCameraAccess)
{
    auto target = addCamera();

    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkRolePermissionsChange)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_FALSE(accessProvider()->hasAccess(role, target));

    role.permissions = Qn::GlobalAccessAllMediaPermission;
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkAccessToOwnLayout)
{
    auto target = createLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, checkParentIdChange)
{
    /* We cannot test parent id change from null to user's as it never happens
     * (and therefore no signal is sent from QnResource in this case). */
    auto user = addUser(Qn::NoGlobalPermissions);
    auto target = createLayout();
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    target->setParentId(QnUuid());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectPermissionsResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}
