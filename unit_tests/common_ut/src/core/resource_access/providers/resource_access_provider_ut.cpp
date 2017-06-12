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
#include <test_support/resource/storage_resource_stub.h>
#include <test_support/resource/camera_resource_stub.h>
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
        return resourceAccessProvider();
    }

    QnLayoutResourcePtr addLayoutForVideoWall(const QnVideoWallResourcePtr& videoWall)
    {
        auto layout = createLayout();
        layout->setParentId(videoWall->getId());
        resourcePool()->addResource(layout);
        return layout;
    }
};

TEST_F(QnResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
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
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kShared);
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughBothChildren)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(QnResourceAccessProviderTest, checkAccessAdding)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccessValue(user, camera, kShared);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    awaitAccessValue(user, camera, kPermissions);
    user->setRawPermissions(Qn::GlobalAdminPermission);
}

TEST_F(QnResourceAccessProviderTest, checkAccessRemoving)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
    awaitAccessValue(user, camera, kShared);
    user->setRawPermissions(Qn::NoGlobalPermissions);
    awaitAccessValue(user, camera, kNone);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
}

TEST_F(QnResourceAccessProviderTest, checkDuplicatedSignals)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    awaitAccessValue(user, camera, kNone);

    /* Here we should not receive the signal. */
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());

    /* And here - should. */
    user->setRawPermissions(Qn::NoGlobalPermissions);
}

TEST_F(QnResourceAccessProviderTest, checkSequentialAccessAdding)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    auto user = addUser(Qn::NoGlobalPermissions);

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::videowall);
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::layout);
    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);

    sharedIds << camera->getId();
    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::shared);
    sharedResourcesManager()->setSharedResources(user, sharedIds);

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::permissions);
    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
}

TEST_F(QnResourceAccessProviderTest, checkSequentialAccessRemoving)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    auto user = createUser(Qn::GlobalAccessAllMediaPermission);
    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::permissions);
    resourcePool()->addResource(user);   //permissions

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId() << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::shared);
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission); // shared

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::layout);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << sharedLayout->getId());

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::videowall);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());

    awaitAccessValue(user, camera, QnAbstractResourceAccessProvider::Source::none);
    resourcePool()->removeResource(videoWall);
}

TEST_F(QnResourceAccessProviderTest, checkAccessProviders)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    QnResourceList providers;

    /* Now function will return full list of all providers. */
    QnResourceList expectedProviders;

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    expectedProviders << videoWall << videoWallLayout;
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(expectedProviders, providers);

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedProviders.prepend(sharedLayout);
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(expectedProviders, providers);

    sharedIds << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    accessProvider()->accessibleVia(user, camera, &providers);
    ASSERT_EQ(expectedProviders, providers);
}


TEST_F(QnResourceAccessProviderTest, checkAccessLevels)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    QSet<QnAbstractResourceAccessProvider::Source> expectedLevels;
    expectedLevels << QnAbstractResourceAccessProvider::Source::videowall;
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedLevels << QnAbstractResourceAccessProvider::Source::layout;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    sharedIds << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedLevels << QnAbstractResourceAccessProvider::Source::shared;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    user->setRawPermissions(Qn::GlobalAdminPermission);
    expectedLevels << QnAbstractResourceAccessProvider::Source::permissions;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));
}
