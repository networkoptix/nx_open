// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource_access/providers/direct_base_access_provider_test_fixture.h>
#include <core/resource_access/providers/shared_layout_item_access_provider.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx/vms/api/data/user_role_data.h>

namespace nx::core::access {
namespace test {

class DirectSharedLayoutItemAccessProviderTest: public DirectBaseAccessProviderTestFixture
{
protected:
    virtual AbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new SharedLayoutItemAccessProvider(Mode::direct, systemContext());
    }
};

TEST_F(DirectSharedLayoutItemAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, checkDefaultCamera)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, checkSharedCamera)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, checkSource)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_EQ(accessProvider()->accessibleVia(user, target), Source::layout);
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, checkSharedServer)
{
    auto target = addServer();
    auto user = addUser(GlobalPermission::none);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    /* User should get access to statistics via shared layouts. */
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, layoutMadeShared)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
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

TEST_F(DirectSharedLayoutItemAccessProviderTest, layoutStopSharing)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    auto layout = addLayout();

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, layoutItemAdded)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    ASSERT_FALSE(accessProvider()->hasAccess(user, target));

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, layoutItemRemoved)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
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

TEST_F(DirectSharedLayoutItemAccessProviderTest, layoutAdded)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    auto layout = createLayout();
    ASSERT_TRUE(layout->isShared());
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    resourcePool()->addResource(layout);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, layoutRemoved)
{
    auto target = addCamera();
    auto user = addUser(GlobalPermission::none);
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    resourcePool()->removeResource(layout);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(DirectSharedLayoutItemAccessProviderTest, accessProviders)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::none);
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

} // namespace test
} // namespace nx::core::access
