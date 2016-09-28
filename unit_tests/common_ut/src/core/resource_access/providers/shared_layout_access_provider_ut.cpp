#include <core/resource_access/providers/access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_layout_access_provider.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx_ec/data/api_user_group_data.h>

class QnSharedLayoutAccessProviderTest: public QnAccessProviderTestFixture
{
protected:
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnSharedLayoutAccessProvider();
    }
};

TEST_F(QnSharedLayoutAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnSharedLayoutAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnSharedLayoutAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, checkSharedCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, checkSharedServer)
{
    auto target = addServer();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    qnResourceAccessManager->setAccessibleResources(user, QSet<QnUuid>() << layout->getId());

    /* User should get access to statistics via shared layouts. */
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}
