#include <core/resource_access/providers/access_provider_test_fixture.h>
#include <core/resource_access/providers/direct_resource_access_provider.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_group_data.h>

class QnDirectResourceAccessProviderTest: public QnAccessProviderTestFixture
{
protected:
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

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkDirectAccessLayout)
{
    auto target = addLayout();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccess(user, target, true);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
}

TEST_F(QnDirectResourceAccessProviderTest, checkAccessRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << target->getId());
    awaitAccess(user, target, false);
    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>());
}
