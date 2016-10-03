#include <core/resource_access/providers/access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_layout_access_provider.h>

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>

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

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_EQ(accessProvider()->accessibleVia(user, target),
        QnAbstractResourceAccessProvider::Source::layout);
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

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    /* User should get access to statistics via shared layouts. */
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, layoutMadeShared)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    layout->setParentId(QnUuid());
    ASSERT_TRUE(layout->isShared());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, layoutStopSharing)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, layoutItemAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, layoutItemRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    layout->removeItem(item);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, layoutAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = createLayout();
    ASSERT_TRUE(layout->isShared());
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    qnResPool->addResource(layout);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnSharedLayoutAccessProviderTest, layoutRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    qnResPool->removeResource(layout);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}
