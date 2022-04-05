// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource_access/providers/cached_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx/vms/api/data/user_role_data.h>

namespace {

static const auto kSource = nx::core::access::Source::shared;

} // namespace

namespace nx::core::access {
namespace test {

class CachedSharedResourceAccessProviderTest: public CachedBaseAccessProviderTestFixture
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
        return new SharedResourceAccessProvider(Mode::cached, commonModule());
    }
};

TEST_F(CachedSharedResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, target), kSource);
}

TEST_F(CachedSharedResourceAccessProviderTest, checkDirectAccessCamera)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkDirectAccessUser)
{
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << user->getId());

    /* We can share only layout and media resources. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, user));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkDirectAccessLayout)
{
    auto target = addLayout();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkDirectAccessOwnedLayout)
{
    auto target = addLayout();
    auto user = addUser(GlobalPermission::none);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Only shared layouts may be accessible through sharing. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkLayoutMadeShared)
{
    auto target = addLayout();
    auto user = addUser(GlobalPermission::none);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Here layout became shared. */
    target->setParentId(QnUuid());
    ASSERT_TRUE(target->isShared());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(CachedSharedResourceAccessProviderTest, checkAccessAdded)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    expectAccessGranted(user, target);
}

TEST_F(CachedSharedResourceAccessProviderTest, checkAccessRemoved)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    expectAccessGranted(user, target, true);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
    expectAccessGranted(user, target, false);
}

// TODO: Add test for user role chains!!

TEST_F(CachedSharedResourceAccessProviderTest, checkRoleAccessGranted)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(GlobalPermission::none);
    user->setUserRoleIds({role.id});
    expectAccessGranted(user, target, false);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
    expectAccessGranted(user, target, true);
}

TEST_F(CachedSharedResourceAccessProviderTest, checkUserRoleChanged)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
    auto user = addUser(GlobalPermission::none);
    expectAccessGranted(user, target, false);
    user->setUserRoleIds({role.id});
    expectAccessGranted(user, target, true);
}

TEST_F(CachedSharedResourceAccessProviderTest, checkUserAndRolesCombinedPermissions)
{
    auto cameraOfParentRole1 = addCamera();
    auto cameraOfParentRole2 = addCamera();
    auto cameraOfInheridedRole = addCamera();
    auto cameraOfUser = addCamera();
    auto cameraOfNoOne = addCamera();

    auto parentRole1 = createRole(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(parentRole1, {cameraOfParentRole1->getId()});
    expectAccessGranted(parentRole1, cameraOfUser, false);
    expectAccessGranted(parentRole1, cameraOfParentRole1, true);
    expectAccessGranted(parentRole1, cameraOfParentRole2, false);
    expectAccessGranted(parentRole1, cameraOfInheridedRole, false);
    expectAccessGranted(parentRole1, cameraOfNoOne, false);

    auto parentRole2 = createRole(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(parentRole2, {cameraOfParentRole2->getId()});
    expectAccessGranted(parentRole2, cameraOfUser, false);
    expectAccessGranted(parentRole2, cameraOfParentRole1, false);
    expectAccessGranted(parentRole2, cameraOfParentRole2, true);
    expectAccessGranted(parentRole2, cameraOfInheridedRole, false);
    expectAccessGranted(parentRole2, cameraOfNoOne, false);

    auto inheritedRole = createRole(GlobalPermission::none, {parentRole1.id, parentRole2.id});
    sharedResourcesManager()->setSharedResources(inheritedRole, {cameraOfInheridedRole->getId()});
    expectAccessGranted(inheritedRole, cameraOfUser, false);
    expectAccessGranted(inheritedRole, cameraOfParentRole1, true);
    expectAccessGranted(inheritedRole, cameraOfParentRole2, true);
    expectAccessGranted(inheritedRole, cameraOfInheridedRole, true);
    expectAccessGranted(inheritedRole, cameraOfNoOne, false);

    auto user = addUser(GlobalPermission::none, "vasily");
    expectAccessGranted(user, cameraOfUser, false);
    expectAccessGranted(user, cameraOfParentRole1, false);
    expectAccessGranted(user, cameraOfParentRole2, false);
    expectAccessGranted(user, cameraOfInheridedRole, false);
    expectAccessGranted(user, cameraOfNoOne, false);

    sharedResourcesManager()->setSharedResources(user, {cameraOfUser->getId()});
    expectAccessGranted(user, cameraOfUser, true);
    expectAccessGranted(user, cameraOfParentRole1, false);
    expectAccessGranted(user, cameraOfParentRole2, false);
    expectAccessGranted(user, cameraOfInheridedRole, false);
    expectAccessGranted(user, cameraOfNoOne, false);

    user->setUserRoleIds({parentRole1.id});
    expectAccessGranted(user, cameraOfUser, true);
    expectAccessGranted(user, cameraOfParentRole1, true);
    expectAccessGranted(user, cameraOfParentRole2, false);
    expectAccessGranted(user, cameraOfInheridedRole, false);
    expectAccessGranted(user, cameraOfNoOne, false);

    user->setUserRoleIds({parentRole1.id, parentRole2.id});
    expectAccessGranted(user, cameraOfUser, true);
    expectAccessGranted(user, cameraOfParentRole1, true);
    expectAccessGranted(user, cameraOfParentRole2, true);
    expectAccessGranted(user, cameraOfInheridedRole, false);
    expectAccessGranted(user, cameraOfNoOne, false);

    user->setUserRoleIds({inheritedRole.id});
    expectAccessGranted(user, cameraOfUser, true);
    expectAccessGranted(user, cameraOfParentRole1, true);
    expectAccessGranted(user, cameraOfParentRole2, true);
    expectAccessGranted(user, cameraOfInheridedRole, true);
    expectAccessGranted(user, cameraOfNoOne, false);

    inheritedRole.parentRoleIds = {};
    userRolesManager()->addOrUpdateUserRole(inheritedRole);
    expectAccessGranted(user, cameraOfUser, true);
    expectAccessGranted(user, cameraOfParentRole1, false);
    expectAccessGranted(user, cameraOfParentRole2, false);
    expectAccessGranted(user, cameraOfInheridedRole, true);
    expectAccessGranted(user, cameraOfNoOne, false);

    user->setUserRoleIds({});
    expectAccessGranted(user, cameraOfUser, true);
    expectAccessGranted(user, cameraOfParentRole1, false);
    expectAccessGranted(user, cameraOfParentRole2, false);
    expectAccessGranted(user, cameraOfInheridedRole, false);
    expectAccessGranted(user, cameraOfNoOne, false);
}

TEST_F(CachedSharedResourceAccessProviderTest, checkRoleRemoved)
{
    auto target = addCamera();

    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(GlobalPermission::none);
    user->setUserRoleIds({role.id});
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
    expectAccessGranted(user, target, true);
    userRolesManager()->removeUserRole(role.id);
    expectAccessGranted(user, target, false);
}

TEST_F(CachedSharedResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}

} // namespace test
} // namespace nx::core::access
