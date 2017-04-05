#include <core/resource_access/providers/base_access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_role_data.h>

namespace {

static const auto kSource = QnAbstractResourceAccessProvider::Source::shared;

}

class QnSharedResourceAccessProviderTest: public QnBaseAccessProviderTestFixture
{
protected:
    void awaitAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value = true)
    {
        if (value)
            awaitAccessValue(subject, resource, kSource);
        else
            awaitAccessValue(subject, resource, QnAbstractResourceAccessProvider::Source::none);
    }

    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnSharedResourceAccessProvider(commonModule());
    }
};

TEST_F(QnSharedResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnSharedResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnSharedResourceAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedResourceAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, target), kSource);
}

TEST_F(QnSharedResourceAccessProviderTest, checkDirectAccessCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedResourceAccessProviderTest, checkDirectAccessUser)
{
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << user->getId());

    /* We can share only layout and media resources. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnSharedResourceAccessProviderTest, checkDirectAccessLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedResourceAccessProviderTest, checkDirectAccessOwnedLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Only shared layouts may be accessible through sharing. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedResourceAccessProviderTest, checkLayoutMadeShared)
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

TEST_F(QnSharedResourceAccessProviderTest, checkAccessAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccess(user, target);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
}

TEST_F(QnSharedResourceAccessProviderTest, checkAccessRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << target->getId());
    awaitAccess(user, target, false);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
}

TEST_F(QnSharedResourceAccessProviderTest, checkRoleAccessGranted)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(Qn::NoGlobalPermissions);
    user->setUserRoleId(role.id);
    awaitAccess(user, target, true);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
}

TEST_F(QnSharedResourceAccessProviderTest, checkUserRoleChanged)
{
    auto target = addCamera();

    auto role = createRole(Qn::NoGlobalPermissions);
    userRolesManager()->addOrUpdateUserRole(role);
    sharedResourcesManager()->setSharedResources(role, QSet<QnUuid>() << target->getId());
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccess(user, target, true);
    user->setUserRoleId(role.id);
}

TEST_F(QnSharedResourceAccessProviderTest, checkRoleRemoved)
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

TEST_F(QnSharedResourceAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}
