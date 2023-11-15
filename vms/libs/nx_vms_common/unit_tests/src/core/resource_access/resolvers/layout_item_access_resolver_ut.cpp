// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <iostream>
#include <memory>

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resolvers/layout_item_access_resolver.h>
#include <core/resource_access/resolvers/own_resource_access_resolver.h>
#include <core/resource_access/resolvers/videowall_item_access_resolver.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

#include "private/resource_access_resolver_test_fixture.h"

namespace nx::core::access {
namespace test {

static constexpr AccessRights kVideoWallControlAccessRights{AccessRight::edit};

using namespace nx::vms::api;

class LayoutItemAccessResolverTest: public ResourceAccessResolverTestFixture
{
public:
    virtual AbstractResourceAccessResolver* createResolver() const override
    {
        const auto ownResolver = new OwnResourceAccessResolver(
            manager.get(), systemContext()->globalPermissionsWatcher(), /*parent*/ manager.get());
        const auto videowallResolver = new VideowallItemAccessResolver(
            ownResolver, resourcePool(), /*parent*/ manager.get());
        return new LayoutItemAccessResolver(videowallResolver, resourcePool());
    }
};

TEST_F(LayoutItemAccessResolverTest, noAccess)
{
    const auto camera = createCamera();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, notApplicableResource)
{
    const auto user = createUser(kPowerUsersGroupId);
    manager->setOwnResourceAccessMap(kTestSubjectId, {{user->getId(), AccessRight::view}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, user), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, layoutAccess)
{
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::view | AccessRight::viewArchive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), testRights);
}

TEST_F(LayoutItemAccessResolverTest, noPrivateLayoutAccess)
{
    const auto layout = addLayout();
    const auto user = addUser(kPowerUsersGroupId);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), AccessRight::view}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, noVideowallLayoutDirectAccess)
{
    const auto videowall = addVideoWall();
    const auto layout = addLayoutForVideoWall(videowall);

    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), kFullAccessRights}});
    EXPECT_EQ(resolver->accessRights(kTestSubjectId, videowall), kNoAccessRights);
    EXPECT_EQ(resolver->accessRights(kTestSubjectId, layout), kNoAccessRights);

    manager->setOwnResourceAccessMap(kTestSubjectId,
        {{layout->getId(), kFullAccessRights}, {videowall->getId(), kVideoWallControlAccessRights}});
    EXPECT_EQ(resolver->accessRights(kTestSubjectId, videowall), kVideoWallControlAccessRights);
    EXPECT_EQ(resolver->accessRights(kTestSubjectId, layout), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, itemAccessViaSharedLayout)
{
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);

    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), kFullAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kFullAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), kViewAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, itemAccessViaVideowall)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), kViewAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, resolvedAccessIsAccumulated)
{
    auto videowall = addVideoWall();
    auto videowallLayout = addLayoutForVideoWall(videowall);

    auto sharedLayout1 = addLayout();
    ASSERT_TRUE(sharedLayout1->isShared());

    auto sharedLayout2 = addLayout();
    ASSERT_TRUE(sharedLayout2->isShared());

    auto camera = addCamera();
    addToLayout(sharedLayout1, camera);
    addToLayout(sharedLayout2, camera);
    addToLayout(videowallLayout, camera);

    manager->setOwnResourceAccessMap(kTestSubjectId, {
        {camera->getId(), AccessRight::edit},
        {sharedLayout1->getId(), AccessRight::viewArchive},
        {sharedLayout2->getId(), AccessRight::userInput},
        {videowall->getId(), kVideoWallControlAccessRights}});

    const AccessRights expectedRights =
        AccessRight::view //< From `videowall`.
        | AccessRight::edit //< From direct access rights to `camera`.
        | AccessRight::viewArchive //< From `sharedLayout1`
        | AccessRight::userInput; //< From `sharedLayout2`

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), expectedRights);
}

TEST_F(LayoutItemAccessResolverTest, noItemAccessViaPrivateLayout)
{
    auto layout = addLayout();
    const auto user = addUser(kPowerUsersGroupId);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), kNoAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, basicNotificationSignals)
{
    const auto layout = addLayout();

    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), AccessRight::view}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Change.
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(),
        AccessRight::view | AccessRight::viewArchive}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Remove.
    manager->removeSubjects({kTestSubjectId});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Clear.
    manager->clear();
    NX_ASSERT_SIGNAL(resourceAccessReset);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(LayoutItemAccessResolverTest, layoutSharingStarted)
{
    auto layout = addLayout();

    const auto user = addUser(kPowerUsersGroupId);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    layout->setParentId({});
    ASSERT_TRUE(layout->isShared());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);
}

TEST_F(LayoutItemAccessResolverTest, layoutSharingStopped)
{
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);

    const auto user = addUser(kPowerUsersGroupId);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, sharedLayoutItemAdded)
{
    auto camera = addCamera();
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    addToLayout(layout, camera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);
}

TEST_F(LayoutItemAccessResolverTest, sharedLayoutItemRemoved)
{
    auto camera = addCamera();
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    const auto itemId = addToLayout(layout, camera);

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);

    layout->removeItem(itemId);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, noAccessToDesktopCameraOnSharedLayout)
{
    auto user = addUser(kPowerUsersGroupId);
    auto desktopCamera1 = addDesktopCamera(user);
    auto desktopCamera2 = createDesktopCamera(user);
    auto layout = addLayout();
    addToLayout(layout, desktopCamera1); //< In the pool.
    addToLayout(layout, desktopCamera2); //< Not in the pool.
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera1), kNoAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera2), kNoAccessRights);

    const auto map1 = resolver->resourceAccessMap(kTestSubjectId);
    ASSERT_FALSE(map1.contains(desktopCamera1->getId()));
    ASSERT_TRUE(map1.contains(desktopCamera2->getId())); //< Resolver doesn't know it's desktop cam.

    resourcePool()->addResource(desktopCamera2);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera1), kNoAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera2), kNoAccessRights);

    const auto map2 = resolver->resourceAccessMap(kTestSubjectId);
    ASSERT_FALSE(map2.contains(desktopCamera1->getId()));
    ASSERT_FALSE(map2.contains(desktopCamera2->getId())); //< Now resolver knows it's desktop cam.
}

TEST_F(LayoutItemAccessResolverTest, videowallLayoutItemAdded)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    auto camera = addCamera();

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    addToLayout(layout, camera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, videowallLayoutItemRemoved)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    auto camera = addCamera();

    const auto itemId = addToLayout(layout, camera);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);

    layout->removeItem(itemId);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, layoutWithCameraAddedToVideowall)
{
    auto videowall = addVideoWall();
    auto camera = addCamera();
    auto layout = createLayout();
    layout->setParentId(videowall->getId());
    addToLayout(layout, camera);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    resourcePool()->addResource(layout);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, cameraAddedToVideowallLayout)
{
    auto videowall = addVideoWall();
    auto camera = addCamera();
    auto layout = addLayoutForVideoWall(videowall);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    addToLayout(layout, camera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, pushMyScreen)
// Effectively the same as `cameraAddedToVideowallLayout`, but for a desktop camera.
{
    auto user = addUser(kPowerUsersGroupId);
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    auto desktopCamera = addDesktopCamera(user);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), kVideoWallControlAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera), kNoAccessRights);

    addToLayout(layout, desktopCamera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, cameraDroppedOnVideoWall_3_0)
{
    auto camera = addCamera();
    auto videowall = addVideoWall();

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // What's going on on drop.
    auto layout = createLayout();
    addToLayout(layout, camera);
    resourcePool()->addResource(layout);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    layout->setParentId(videowall->getId());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, cameraDroppedOnVideoWall_3_1)
{
    auto camera = addCamera();
    auto videowall = addVideoWall();

    QnVideoWallItem vwitem;
    vwitem.uuid = QnUuid::createUuid();
    videowall->items()->addItem(vwitem);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // What's going on on drop.
    auto layout = createLayout();
    addToLayout(layout, camera);
    layout->setParentId(videowall->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    vwitem.layout = layout->getId();
    videowall->items()->updateItem(vwitem);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, parentlessLayoutBecomesVideowallLayout)
// Not a standard scenario, but should work nonetheless.
{
    auto videowall = addVideoWall();
    auto camera = addCamera();
    auto layout = createLayout();

    addToLayout(layout, camera);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    resourcePool()->addResource(layout);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    layout->setParentId(videowall->getId());

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, sharedLayoutAdded)
{
    auto layout = createLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    // Layout is not in the pool yet, its changes are not tracked.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), kNoAccessRights);

    resourcePool()->addResource(layout);

    // Layout is in the pool now.
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), testRights);

    // And its changes are now tracked.
    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), testRights);
}

TEST_F(LayoutItemAccessResolverTest, sharedLayoutRemoved)
{
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // This check is required for the following signal checks to succeed.
    // Otherwise the subject access map is not cached and notification signals are not sent.
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), testRights);

    auto camera = addCamera();
    addToLayout(layout, camera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);

    resourcePool()->removeResource(layout);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    // Layout changes are not tracked now.
    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, videowallLayoutRemoved)
{
    auto camera = addCamera();
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    addToLayout(layout, camera);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);

    QnVideoWallItem item = *videowall->items()->getItems().cbegin();
    item.layout = {};
    videowall->items()->updateItem(item);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    resourcePool()->removeResource(layout);

    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, videowallLayoutRemovedFromThePoolFirst)
{
    auto camera = addCamera();
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    addToLayout(layout, camera);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kViewAccessRights);

    resourcePool()->removeResource(layout);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);

    QnVideoWallItem item = *videowall->items()->getItems().cbegin();
    item.layout = {};
    videowall->items()->updateItem(item);

    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), kNoAccessRights);
}

TEST_F(LayoutItemAccessResolverTest, videowallMatrixLayouts)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, AccessRight::edit}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto layout1 = addLayout();
    layout1->setParentId(videowall->getId());
    auto layout2 = addLayout();
    layout2->setParentId(videowall->getId());
    auto layout3 = addLayout();
    layout3->setParentId(videowall->getId());

    const auto cameras = addCameras(3);
    addToLayout(layout1, cameras[0]);
    addToLayout(layout2, cameras[1]);
    addToLayout(layout3, cameras[2]);

    const auto item1 = addVideoWallItem(videowall, {});
    const auto item2 = addVideoWallItem(videowall, {});
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    const auto item3 = addVideoWallItem(videowall, layout3);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[0]), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[1]), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[2]), AccessRight::view);

    QnVideoWallMatrix matrix;
    matrix.uuid = QnUuid::createUuid();
    matrix.layoutByItem[item1] = layout1->getId();
    matrix.layoutByItem[item2] = layout2->getId();
    matrix.layoutByItem[item3] = layout3->getId();
    videowall->matrices()->addItem(matrix);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[0]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[1]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[2]), AccessRight::view);

    ASSERT_TRUE(changeVideoWallItem(videowall, item3, {}));
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[0]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[1]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[2]), AccessRight::view);

    matrix.layoutByItem[item3] = {};
    videowall->matrices()->addOrUpdateItem(matrix);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[0]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[1]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[2]), AccessRights{});

    videowall->items()->removeItem(item1);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[0]), AccessRights{});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[1]), AccessRight::view);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[2]), AccessRights{});

    videowall->matrices()->removeItem(matrix);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[0]), AccessRights{});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[1]), AccessRights{});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, cameras[2]), AccessRights{});
}

TEST_F(LayoutItemAccessResolverTest, noSignalsForPrivateLayoutChanges)
{
    auto layout = createLayout();
    const auto user = addUser(kPowerUsersGroupId);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    // The signal `resourceAccessChanged` is sent after explicit changing of access rights
    // to any resource, even a layout that is private and/or not in the pool.
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto camera = addCamera();
    addToLayout(layout, camera);

    // No signal for adding an item.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    resourcePool()->addResource(layout);

    // No signal for adding the layout.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    auto webPage = addWebPage();
    const auto itemId = addToLayout(layout, webPage);

    // No signal for adding an item.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    layout->removeItem(itemId);

    // No signal for removing an item.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    resourcePool()->removeResource(layout);

    // No signal for removing the layout.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(LayoutItemAccessResolverTest, noSignalsForLayoutWithoutAccessRights)
{
    auto layout = createLayout();
    ASSERT_TRUE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    // No signal for adding an item.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    resourcePool()->addResource(layout);

    // No signal for adding the layout.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    auto webPage = addWebPage();
    const auto itemId = addToLayout(layout, webPage);

    // No signal for adding an item.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    layout->removeItem(itemId);

    // No signal for removing an item.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    resourcePool()->removeResource(layout);

    // No signal for removing the layout.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(LayoutItemAccessResolverTest, accessDetailsViaSharedLayout)
{
    auto layout = addLayout();
    layout->setName("Layout");
    ASSERT_TRUE(layout->isShared());

    auto camera = addCamera();
    camera->setName("Camera");
    addToLayout(layout, camera);

    const AccessRights testRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {layout}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {layout}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::viewArchive),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewArchive),
        ResourceAccessDetails());
}

TEST_F(LayoutItemAccessResolverTest, accessDetailsViaVideowallLayout)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    layout->setName("Layout");
    videowall->setName("Videowall");

    auto camera = addCamera();
    camera->setName("Camera");
    addToLayout(layout, camera);

    manager->setOwnResourceAccessMap(
        kTestSubjectId, {{videowall->getId(), kVideoWallControlAccessRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::viewArchive),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewArchive),
        ResourceAccessDetails());

    // Direct access rights given to a videowall layout should be ignored.

    manager->setOwnResourceAccessMap(kTestSubjectId, {
        {videowall->getId(), kVideoWallControlAccessRights},
        {layout->getId(), kFullAccessRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::edit),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::view),
        ResourceAccessDetails({ {kTestSubjectId, {videowall}} }));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::edit),
        ResourceAccessDetails());
}

TEST_F(LayoutItemAccessResolverTest, accessDetailsViaMultipleLayouts)
{
    auto videowall1 = addVideoWall();
    auto layout1 = addLayoutForVideoWall(videowall1);
    layout1->setName("VideowallLayout1");
    videowall1->setName("Videowall1");

    auto videowall2 = addVideoWall();
    auto layout2 = addLayoutForVideoWall(videowall2);
    layout2->setName("VideowallLayout2");
    videowall2->setName("Videowall2");

    auto layout3 = addLayout();
    layout3->setName("SharedLayout1");

    auto layout4 = addLayout();
    layout4->setName("SharedLayout2");

    auto camera = addCamera();
    camera->setName("Camera");
    addToLayout(layout1, camera);
    addToLayout(layout2, camera);
    addToLayout(layout3, camera);
    addToLayout(layout4, camera);

    manager->setOwnResourceAccessMap(kTestSubjectId, {
        {videowall1->getId(), kVideoWallControlAccessRights},
        {videowall2->getId(), kVideoWallControlAccessRights},
        {layout3->getId(), kViewAccessRights},
        {layout4->getId(), kViewAccessRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {videowall1, videowall2, layout3, layout4}}}));
}

} // namespace test
} // namespace nx::core::access
