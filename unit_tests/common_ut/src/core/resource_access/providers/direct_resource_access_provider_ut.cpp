#include <core/resource_access/providers/access_provider_test_fixture.h>
#include <core/resource_access/providers/direct_resource_access_provider.h>

#include <core/resource_access/shared_resources_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_group_data.h>

namespace {

static const auto kSource = QnAbstractResourceAccessProvider::Source::shared;

}

class QnDirectResourceAccessProviderTest: public QnAccessProviderTestFixture
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
        return new QnDirectResourceAccessProvider();
    }
};

TEST_F(QnDirectResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, target), kSource);
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessUser)
{
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << user->getId());

    /* We can share only layout and media resources. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessOwnedLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Only shared layouts may be accessible through sharing. */
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkLayoutMadeShared)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    target->setParentId(user->getId());
    ASSERT_FALSE(target->isShared());
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());

    /* Here layout became shared. */
    target->setParentId(QnUuid());
    ASSERT_TRUE(target->isShared());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccess(user, target);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << target->getId());
    awaitAccess(user, target, false);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>());
}
