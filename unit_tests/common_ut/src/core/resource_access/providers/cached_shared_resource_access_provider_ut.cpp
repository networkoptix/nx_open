#include <core/resource_access/providers/cached_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_role_data.h>

using namespace nx::core::access;

namespace {

static const auto kSource = Source::shared;

}

class QnCachedSharedResourceAccessProviderTest: public QnCachedBaseAccessProviderTestFixture
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
        return new QnSharedResourceAccessProvider(Mode::cached, commonModule());
    }
};

TEST_F(QnCachedSharedResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, target), kSource);
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkDirectAccessCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkDirectAccessUser)
{
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << user->getId());

    /* We can share only layout and media resources. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkDirectAccessLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkDirectAccessOwnedLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Only shared layouts may be accessible through sharing. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkLayoutMadeShared)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Here layout became shared. */
    target->setParentId(QnUuid());
    ASSERT_TRUE(target->isShared());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkAccessAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccess(user, target);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkAccessRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    awaitAccess(user, target, false);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkRoleAccessGranted)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(Qn::NoGlobalPermissions);
    user->setUserRoleId(role.id);
    awaitAccess(user, target, true);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkUserRoleChanged)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccess(user, target, true);
    user->setUserRoleId(role.id);
}

TEST_F(QnCachedSharedResourceAccessProviderTest, checkRoleRemoved)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(Qn::NoGlobalPermissions);
    user->setUserRoleId(role.id);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
    awaitAccess(user, target, false);
    userRolesManager()->removeUserRole(role.id);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}
