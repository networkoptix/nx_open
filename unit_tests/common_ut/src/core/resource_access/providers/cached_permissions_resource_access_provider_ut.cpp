#include <core/resource_access/providers/cached_base_access_provider_test_fixture.h>
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

class QnCachedPermissionsResourceAccessProviderTest: public QnCachedBaseAccessProviderTestFixture
{
protected:
    void awaitAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value = true)
    {
        if (value)
            awaitAccessValue(subject, resource, kSource);
        else
            awaitAccessValue(subject, resource, Source::none);
    }

    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnPermissionsResourceAccessProvider(
            Mode::cached,
            commonModule());
    }
};

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkSource)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kSource);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addLayout();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addWebPage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addVideoWall();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addServer();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto server = addServer();
    auto target = addStorage(server);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByName)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName2);

    auto user = addUser(Qn::GlobalAdminPermission);
    auto user2 = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);

    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
    ASSERT_TRUE(accessProvider()->hasAccess(user2, camera));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByPermissions)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName);

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = addWebPage();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkServerAccess)
{
    auto target = addServer();

    /* Servers are now controlled via the same 'All Media' flag. */
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkStorageAccess)
{
    auto server = addServer();
    auto target = addStorage(server);

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkLayoutAccess)
{
    auto target = addLayout();

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkUserAccess)
{
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    auto target = addUser(Qn::GlobalAccessAllMediaPermission, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkVideoWallAccess)
{
    auto target = addVideoWall();

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkUserRoleChangeAccess)
{
    auto target = addCamera();

    auto user = addUser(Qn::NoGlobalPermissions);
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setEnabled(false);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, nonPoolResourceAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitNewCameraAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto camera = createCamera();
    awaitAccess(user, camera);
    resourcePool()->addResource(camera);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitRemovedCameraAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto camera = addCamera();
    awaitAccess(user, camera, false);
    resourcePool()->removeResource(camera);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitNewUserAccess)
{
    auto camera = addCamera();
    auto user = createUser(Qn::GlobalAdminPermission);
    awaitAccess(user, camera);
    resourcePool()->addResource(user);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitRemovedUserAccess)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    awaitAccess(user, camera, false);
    resourcePool()->removeResource(user);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitPermissionsChangedAccess)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccess(user, camera);
    user->setRawPermissions(Qn::GlobalAdminPermission);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, suppressDuplicatedSignals)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    awaitAccess(user, camera, false);

    /* Here we should NOT receive the signal */
    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);

    /* Here we should receive the 'false' value. */
    resourcePool()->removeResource(user);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkRoleCameraAccess)
{
    auto target = addCamera();

    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkRolePermissionsChange)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_FALSE(accessProvider()->hasAccess(role, target));

    role.permissions = Qn::GlobalAccessAllMediaPermission;
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(role, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitAddedRoleAccess)
{
    auto target = addCamera();

    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    awaitAccess(role, target, true);
    userRolesManager()->addOrUpdateUserRole(role);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitRemovedRoleAccess)
{
    auto target = addCamera();

    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);
    awaitAccess(role, target, false);

    userRolesManager()->removeUserRole(role.id);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, suppressDuplicatedRoleSignals)
{
    auto target = addCamera();
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);

    awaitAccess(role, target, false);

    /* Here we should NOT receive the signal */
    role.permissions = Qn::GlobalAccessAllMediaPermission | Qn::GlobalExportPermission;
    userRolesManager()->addOrUpdateUserRole(role);

    /* Here we should receive the 'false' value. */
    userRolesManager()->removeUserRole(role.id);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitRoleAccessToAddedCamera)
{
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);

    auto target = createCamera();
    awaitAccess(role, target, true);
    resourcePool()->addResource(target);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitRoleAccessToRemovedCamera)
{
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);

    auto target = addCamera();
    awaitAccess(role, target, false);
    resourcePool()->removeResource(target);
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, awaitUserAccessChangeOnRolePermissionsChange)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);

    auto user = addUser(Qn::GlobalAdminPermission);
    user->setUserRoleId(role.id);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    awaitAccess(user, target, true);
    role.permissions = Qn::GlobalAccessAllMediaPermission;
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkAccessToOwnLayout)
{
    auto target = createLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    resourcePool()->addResource(target);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedPermissionsResourceAccessProviderTest, checkParentIdChange)
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

TEST_F(QnCachedPermissionsResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}
