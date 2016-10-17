#include <core/resource_access/providers/access_provider_test_fixture.h>

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource_stub.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

namespace {

static const auto kPermissions = QnAbstractResourceAccessProvider::Source::permissions;
static const auto kShared = QnAbstractResourceAccessProvider::Source::shared;
static const auto kNone = QnAbstractResourceAccessProvider::Source::none;

}

class QnResourceAccessProviderTest: public QnAccessProviderTestFixture
{
    using base_type = QnAccessProviderTestFixture;
protected:
    virtual void SetUp()
    {
        base_type::SetUp();
        setupAwaitAccess();
    }

    QnResourceAccessProvider* accessProvider() const
    {
        return qnResourceAccessProvider;
    }
};

TEST_F(QnResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughChild)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughSecondChild)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kShared);
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughBothChildren)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(QnResourceAccessProviderTest, checkAccessAdding)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccessValue(user, camera, kShared);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    awaitAccessValue(user, camera, kPermissions);
    user->setRawPermissions(Qn::GlobalAdminPermission);
}

TEST_F(QnResourceAccessProviderTest, checkAccessRemoving)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
    awaitAccessValue(user, camera, kShared);
    user->setRawPermissions(Qn::NoGlobalPermissions);
    awaitAccessValue(user, camera, kNone);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>());
}

TEST_F(QnResourceAccessProviderTest, checkDuplicatedSignals)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    awaitAccessValue(user, camera, kNone);

    /* Here we should not receive the signal. */
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>());

    /* And here - should. */
    user->setRawPermissions(Qn::NoGlobalPermissions);
}

TEST_F(QnResourceAccessProviderTest, checkSequentialAccessAdding)
{
    auto camera = addCamera();

    auto layout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::videowall);
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::layout);
    auto sharedIds = QSet<QnUuid>() << layout->getId();
    qnSharedResourcesManager->setSharedResources(user, sharedIds);

    sharedIds << camera->getId();
    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::shared);
    qnSharedResourcesManager->setSharedResources(user, sharedIds);

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::permissions);
    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
}

TEST_F(QnResourceAccessProviderTest, checkSequentialAccessRemoving)
{
    auto camera = addCamera();
    auto videoWall = addVideoWall();
    auto layout = addLayout();

    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    auto user = createUser(Qn::GlobalAccessAllMediaPermission);
    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::permissions);
    qnResPool->addResource(user);   //permissions

    auto sharedIds = QSet<QnUuid>() << layout->getId() << camera->getId();
    qnSharedResourcesManager->setSharedResources(user, sharedIds);
    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::shared);
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission); // shared

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::layout);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << layout->getId());

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::videowall);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>());

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::none);
    videoWall->items()->removeItem(item);
}

TEST_F(QnResourceAccessProviderTest, checkAccessProviders)
{
    auto camera = addCamera();

    auto layout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    layout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    QnVideoWallItem item;
    item.layout = layout->getId();
    videoWall->items()->addItem(item);

    QnResourceList providers;

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(providers, QnResourceList() << videoWall);

    auto sharedIds = QSet<QnUuid>() << layout->getId();
    qnSharedResourcesManager->setSharedResources(user, sharedIds);
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(providers, QnResourceList() << layout);

    sharedIds << camera->getId();
    qnSharedResourcesManager->setSharedResources(user, sharedIds);
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_TRUE(providers.empty());
}
