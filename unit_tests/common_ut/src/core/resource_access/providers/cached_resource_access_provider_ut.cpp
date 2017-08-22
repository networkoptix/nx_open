#include <core/resource_access/providers/cached_access_provider_test_fixture.h>

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

using namespace nx::core::access;

namespace {

static const auto kPermissions = Source::permissions;
static const auto kShared = Source::shared;
static const auto kNone = Source::none;

}

class QnCachedResourceAccessProviderTest: public QnCachedAccessProviderTestFixture
{
    using base_type = QnCachedAccessProviderTestFixture;
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
};

TEST_F(QnCachedResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnCachedResourceAccessProviderTest, checkAccessThroughChild)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(QnCachedResourceAccessProviderTest, checkAccessThroughSecondChild)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kShared);
}

TEST_F(QnCachedResourceAccessProviderTest, checkAccessThroughBothChildren)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(QnCachedResourceAccessProviderTest, checkAccessAdding)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccessValue(user, camera, kShared);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    awaitAccessValue(user, camera, kPermissions);
    user->setRawPermissions(Qn::GlobalAdminPermission);
}

TEST_F(QnCachedResourceAccessProviderTest, checkAccessRemoving)
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

TEST_F(QnCachedResourceAccessProviderTest, checkDuplicatedSignals)
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

TEST_F(QnCachedResourceAccessProviderTest, checkSequentialAccessAdding)
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

    awaitAccessValue(user, camera, Source::videowall);
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission);

    awaitAccessValue(user, camera, Source::layout);
    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);

    sharedIds << camera->getId();
    awaitAccessValue(user, camera, Source::shared);
    sharedResourcesManager()->setSharedResources(user, sharedIds);

    awaitAccessValue(user, camera, Source::permissions);
    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
}

TEST_F(QnCachedResourceAccessProviderTest, checkSequentialAccessRemoving)
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
    awaitAccessValue(user, camera, Source::permissions);
    resourcePool()->addResource(user);   //permissions

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId() << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    awaitAccessValue(user, camera, Source::shared);
    user->setRawPermissions(Qn::GlobalControlVideoWallPermission); // shared

    awaitAccessValue(user, camera, Source::layout);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << sharedLayout->getId());

    awaitAccessValue(user, camera, Source::videowall);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());

    awaitAccessValue(user, camera, Source::none);
    resourcePool()->removeResource(videoWall);
}

TEST_F(QnCachedResourceAccessProviderTest, checkAccessProviders)
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


TEST_F(QnCachedResourceAccessProviderTest, checkAccessLevels)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    QSet<Source> expectedLevels;
    expectedLevels << Source::videowall;
    auto user = addUser(Qn::GlobalControlVideoWallPermission);

    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedLevels << Source::layout;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    sharedIds << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedLevels << Source::shared;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    user->setRawPermissions(Qn::GlobalAdminPermission);
    expectedLevels << Source::permissions;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));
}
