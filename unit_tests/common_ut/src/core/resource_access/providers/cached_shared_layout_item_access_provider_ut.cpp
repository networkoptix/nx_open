#include <core/resource_access/providers/cached_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_layout_item_access_provider.h>

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx_ec/data/api_user_role_data.h>

using namespace nx::core::access;

class QnCachedSharedLayoutItemAccessProviderTest: public QnCachedBaseAccessProviderTestFixture
{
protected:
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnSharedLayoutItemAccessProvider(Mode::cached, commonModule());
    }
};

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, checkSharedCamera)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_EQ(accessProvider()->accessibleVia(user, target), Source::layout);
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, checkSharedServer)
{
    auto target = addServer();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    /* User should get access to statistics via shared layouts. */
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, layoutMadeShared)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    layout->setParentId(QnUuid());
    ASSERT_TRUE(layout->isShared());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, layoutStopSharing)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, layoutItemAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, layoutItemRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    layout->removeItem(item);

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, layoutAdded)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = createLayout();
    ASSERT_TRUE(layout->isShared());
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    resourcePool()->addResource(layout);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, layoutRemoved)
{
    auto target = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    resourcePool()->removeResource(layout);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnCachedSharedLayoutItemAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    auto layout = addLayout();

    QnLayoutItemData item;
    item.resource.id = camera->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    QnResourceList providers;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(1, providers.size());
    ASSERT_TRUE(providers.contains(layout));
}
