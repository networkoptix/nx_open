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
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

#include "private/resource_access_resolver_test_fixture.h"
#include "../access_rights_types_testing.h"

namespace nx::core::access {
namespace test {

using namespace nx::vms::api;

namespace {

AccessRights videowallItemAccessRights(AccessRights videowallAccessRights)
{
    return videowallAccessRights & ~int(AccessRight::controlVideowall);
}

} // namespace

class LayoutItemAccessResolverTest: public ResourceAccessResolverTestFixture
{
public:
    virtual AbstractResourceAccessResolver* createResolver() const override
    {
        const auto ownResolver = new OwnResourceAccessResolver(
            manager.get(), systemContext()->globalPermissionsWatcher(), /*parent*/ manager.get());
        const auto videowallResolver = new VideowallItemAccessResolver(
            ownResolver, resourcePool(), /*parent*/ ownResolver);
        return new LayoutItemAccessResolver(videowallResolver, resourcePool());
    }
};

TEST_F(LayoutItemAccessResolverTest, noAccess)
{
    const auto camera = createCamera();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, notApplicableResource)
{
    const auto user = createUser(GlobalPermission::admin);
    manager->setOwnResourceAccessMap(kTestSubjectId, {{user->getId(), AccessRight::viewLive}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, user), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, layoutAccess)
{
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::viewLive | AccessRight::listenToAudio;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), testRights);
}

TEST_F(LayoutItemAccessResolverTest, noPrivateLayoutAccess)
{
    const auto layout = addLayout();
    const auto user = addUser(GlobalPermission::admin);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), AccessRight::viewLive}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, noVideowallLayoutDirectAccess)
{
    const auto videowall = addVideoWall();
    const auto layout = addLayoutForVideoWall(videowall);
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), AccessRight::viewLive}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, itemAccessViaLayout)
{
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), testRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), testRights);
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

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;

    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage),
        videowallItemAccessRights(testRights));
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor),
        videowallItemAccessRights(testRights));
}

TEST_F(LayoutItemAccessResolverTest, noItemAccessViaPrivateLayout)
{
    auto layout = addLayout();
    const auto user = addUser(GlobalPermission::admin);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, basicNotificationSignals)
{
    const auto layout = addLayout();

    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), AccessRight::viewLive}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Change.
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(),
        AccessRight::viewLive | AccessRight::listenToAudio}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Remove.
    manager->removeSubjects({ kTestSubjectId });
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Clear.
    manager->clear();
    NX_ASSERT_SIGNAL(resourceAccessReset);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(LayoutItemAccessResolverTest, layoutSharingStarted)
{
    auto layout = addLayout();

    const auto user = addUser(GlobalPermission::admin);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    auto camera = addCamera();
    addToLayout(layout, camera);

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

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

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);

    const auto user = addUser(GlobalPermission::admin);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, sharedLayoutItemAdded)
{
    auto camera = addCamera();
    auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

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

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testRights);

    layout->removeItem(itemId);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, noAccessToDesktopCameraOnSharedLayout)
{
    auto user = addUser(GlobalPermission::admin);
    auto desktopCamera1 = addDesktopCamera(user);
    auto desktopCamera2 = createDesktopCamera(user);
    auto layout = addLayout();
    addToLayout(layout, desktopCamera1); //< In the pool.
    addToLayout(layout, desktopCamera2); //< Not in the pool.
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera1), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera2), AccessRights());

    const auto map1 = resolver->resourceAccessMap(kTestSubjectId);
    ASSERT_FALSE(map1.contains(desktopCamera1->getId()));
    ASSERT_TRUE(map1.contains(desktopCamera2->getId())); //< Resolver doesn't know it's desktop cam.

    resourcePool()->addResource(desktopCamera2);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera1), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera2), AccessRights());

    const auto map2 = resolver->resourceAccessMap(kTestSubjectId);
    ASSERT_FALSE(map2.contains(desktopCamera1->getId()));
    ASSERT_FALSE(map2.contains(desktopCamera2->getId())); //< Now resolver knows it's desktop cam.
}

TEST_F(LayoutItemAccessResolverTest, videowallLayoutItemAdded)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    auto camera = addCamera();

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    addToLayout(layout, camera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
}

TEST_F(LayoutItemAccessResolverTest, videowallLayoutItemRemoved)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    auto camera = addCamera();

    const auto itemId = addToLayout(layout, camera);

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));

    layout->removeItem(itemId);
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
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

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    resourcePool()->addResource(layout);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
}

TEST_F(LayoutItemAccessResolverTest, cameraAddedOnVideowallLayout)
{
    auto videowall = addVideoWall();
    auto camera = addCamera();
    auto layout = addLayoutForVideoWall(videowall);

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    addToLayout(layout, camera);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
}

TEST_F(LayoutItemAccessResolverTest, pushMyScreen)
// Effectively the same as `cameraAddedOnVideowallLayout`, but for a desktop camera.
{
    auto user = addUser(GlobalPermission::admin);
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    auto desktopCamera = addDesktopCamera(user);

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::listenToAudio
        | AccessRight::viewArchive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), testRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera), AccessRights());

    addToLayout(layout, desktopCamera);

    constexpr AccessRights kAllowedRights = AccessRight::viewLive | AccessRight::listenToAudio;

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, desktopCamera), testRights & kAllowedRights);
}

TEST_F(LayoutItemAccessResolverTest, cameraDroppedOnVideoWall_3_0)
{
    auto camera = addCamera();
    auto videowall = addVideoWall();

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // What's going on on drop.
    auto layout = createLayout();
    addToLayout(layout, camera);
    resourcePool()->addResource(layout);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    layout->setParentId(videowall->getId());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
}

TEST_F(LayoutItemAccessResolverTest, cameraDroppedOnVideoWall_3_1)
{
    auto camera = addCamera();
    auto videowall = addVideoWall();

    QnVideoWallItem vwitem;
    vwitem.uuid = QnUuid::createUuid();
    videowall->items()->addItem(vwitem);

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // What's going on on drop.
    auto layout = createLayout();
    addToLayout(layout, camera);
    layout->setParentId(videowall->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    vwitem.layout = layout->getId();
    videowall->items()->updateItem(vwitem);

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
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

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    resourcePool()->addResource(layout);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    layout->setParentId(videowall->getId());

    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera),
        videowallItemAccessRights(testRights));
}

TEST_F(LayoutItemAccessResolverTest, sharedLayoutAdded)
{
    auto layout = createLayout();
    ASSERT_TRUE(layout->isShared());

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto camera = addCamera();
    addToLayout(layout, camera);

    auto webPage = addWebPage();
    addToLayout(layout, webPage);

    // Layout is not in the pool yet, its changes are not tracked.
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, webPage), AccessRights());

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

    const AccessRights testRights = AccessRight::viewLive;
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
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    // Layout changes are not tracked now.
    auto healthMonitor = addServer();
    addToLayout(layout, healthMonitor);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, healthMonitor), AccessRights());
}

TEST_F(LayoutItemAccessResolverTest, noSignalsForPrivateLayoutChanges)
{
    auto layout = createLayout();
    const auto user = addUser(GlobalPermission::admin);
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    const AccessRights testRights = AccessRight::viewLive;
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

    const AccessRights testRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{layout->getId(), testRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {layout}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {layout}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::listenToAudio),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::listenToAudio),
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

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), testRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::listenToAudio),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::listenToAudio),
        ResourceAccessDetails());

    // Direct access rights given to a videowall layout should be ignored.

    manager->setOwnResourceAccessMap(kTestSubjectId, {
        {videowall->getId(), testRights},
        {layout->getId(), testRights | AccessRight::editSettings}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::editSettings),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewLive),
        ResourceAccessDetails({ {kTestSubjectId, {videowall}} }));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::editSettings),
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

    const AccessRights testRights = AccessRight::viewLive
        | AccessRight::controlVideowall;
    manager->setOwnResourceAccessMap(kTestSubjectId, {
        {videowall1->getId(), testRights},
        {videowall2->getId(), testRights},
        {layout3->getId(), testRights},
        {layout4->getId(), testRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {videowall1, videowall2, layout3, layout4}}}));
}

} // namespace test
} // namespace nx::core::access
