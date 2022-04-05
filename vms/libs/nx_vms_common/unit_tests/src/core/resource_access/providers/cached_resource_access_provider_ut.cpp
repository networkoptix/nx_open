// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/providers/cached_access_provider_test_fixture.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/resource/storage_resource_stub.h>

using namespace nx::core::access;

namespace {

static const auto kPermissions = Source::permissions;
static const auto kShared = Source::shared;
static const auto kNone = Source::none;

} // namespace

namespace nx::core::access {
namespace test {

class CachedResourceAccessProviderTest: public CachedAccessProviderTestFixture
{
    using base_type = CachedAccessProviderTestFixture;
protected:
    virtual void SetUp()
    {
        base_type::SetUp();
        setupAwaitAccess();
    }

    ResourceAccessProvider* accessProvider() const
    {
        return resourceAccessProvider();
    }
};

TEST_F(CachedResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    nx::vms::api::UserRoleData userRole;
    QnResourceAccessSubject subject(userRole);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(CachedResourceAccessProviderTest, checkAccessThroughChild)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(CachedResourceAccessProviderTest, checkAccessThroughSecondChild)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kShared);
}

TEST_F(CachedResourceAccessProviderTest, checkAccessThroughBothChildren)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
}

TEST_F(CachedResourceAccessProviderTest, checkAccessAdding)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    expectAccess(user, camera, kShared);
    user->setRawPermissions(GlobalPermission::admin);
    expectAccess(user, camera, kPermissions);
}

TEST_F(CachedResourceAccessProviderTest, checkAccessRemoving)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_EQ(accessProvider()->accessibleVia(user, camera), kPermissions);
    user->setRawPermissions(GlobalPermission::none);
    expectAccess(user, camera, kShared);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
    expectAccess(user, camera, kNone);
}

TEST_F(CachedResourceAccessProviderTest, checkDuplicatedSignals)
{
    auto camera = addCamera();
    auto user = addUser(GlobalPermission::admin);
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    expectAccess(user, camera, kPermissions);

    /* Here we should not receive the signal. */
    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
    expectAccess(user, camera, kPermissions);

    /* And here - should. */
    user->setRawPermissions(GlobalPermission::none);
    expectAccess(user, camera, kNone);
}

TEST_F(CachedResourceAccessProviderTest, checkSequentialAccessAdding)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    auto user = addUser(GlobalPermission::none);

    user->setRawPermissions(GlobalPermission::controlVideowall);
    expectAccess(user, camera, Source::videowall);

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectAccess(user, camera, Source::layout);

    sharedIds << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectAccess(user, camera, Source::shared);

    user->setRawPermissions(GlobalPermission::accessAllMedia);
    expectAccess(user, camera, Source::permissions);
}

TEST_F(CachedResourceAccessProviderTest, checkSequentialAccessRemoving)
{
    auto camera = addCamera();

    auto sharedLayout = addLayout();
    QnLayoutItemData layoutItem;
    layoutItem.resource.id = camera->getId();
    sharedLayout->addItem(layoutItem);

    auto videoWall = addVideoWall();
    auto videoWallLayout = addLayoutForVideoWall(videoWall);
    videoWallLayout->addItem(layoutItem);

    auto user = createUser(GlobalPermission::accessAllMedia);
    resourcePool()->addResource(user);   //permissions
    expectAccess(user, camera, Source::permissions);

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId() << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    user->setRawPermissions(GlobalPermission::controlVideowall); // shared
    expectAccess(user, camera, Source::shared);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>() << sharedLayout->getId());
    expectAccess(user, camera, Source::layout);

    sharedResourcesManager()->setSharedResources(user, QSet<QnUuid>());
    expectAccess(user, camera, Source::videowall);

    resourcePool()->removeResource(videoWall);
    expectAccess(user, camera, Source::none);
}

TEST_F(CachedResourceAccessProviderTest, checkAccessProviders)
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

    auto user = addUser(GlobalPermission::controlVideowall);
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

TEST_F(CachedResourceAccessProviderTest, checkAccessLevels)
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
    auto user = addUser(GlobalPermission::controlVideowall);

    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    auto sharedIds = QSet<QnUuid>() << sharedLayout->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedLevels << Source::layout;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    sharedIds << camera->getId();
    sharedResourcesManager()->setSharedResources(user, sharedIds);
    expectedLevels << Source::shared;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));

    user->setRawPermissions(GlobalPermission::admin);
    expectedLevels << Source::permissions;
    ASSERT_EQ(expectedLevels, accessProvider()->accessLevels(user, camera));
}

} // namespace test
} // namespace nx::core::access
