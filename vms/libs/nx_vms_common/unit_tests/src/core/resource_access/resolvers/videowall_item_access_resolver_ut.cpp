// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resolvers/videowall_item_access_resolver.h>
#include <core/resource_access/resolvers/own_resource_access_resolver.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

#include "private/resource_access_resolver_test_fixture.h"

namespace nx::core::access {
namespace test {

using namespace nx::vms::api;

namespace {

AccessRights videowallItemAccessRights(AccessRights videowallAccessRights)
{
    return videowallAccessRights.testFlag(AccessRight::edit)
        ? AccessRights{AccessRight::view}
        : AccessRights{};
}

} // namespace

class VideowallItemAccessResolverTest: public ResourceAccessResolverTestFixture
{
public:
    virtual AbstractResourceAccessResolver* createResolver() const override
    {
        const auto ownResolver = new OwnResourceAccessResolver(
            manager.get(), systemContext()->globalPermissionsWatcher(), /*parent*/ manager.get());
        return new VideowallItemAccessResolver(ownResolver, resourcePool());
    }

    const AccessRights kTestAccessRights =
        AccessRight::edit | AccessRight::viewArchive;
};

TEST_F(VideowallItemAccessResolverTest, noAccess)
{
    const auto camera = addCamera();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());

    const auto layout = addLayout();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    const auto videowall = addVideoWall();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), AccessRights());

    layout->setParentId(videowall->getId());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
}

TEST_F(VideowallItemAccessResolverTest, notApplicableResource)
{
    const auto user = createUser(kPowerUsersGroupId);
    manager->setOwnResourceAccessMap(kTestSubjectId, {{user->getId(), AccessRight::view}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, user), AccessRights());
}

TEST_F(VideowallItemAccessResolverTest, videowallAccessRights)
{
    auto videowall = addVideoWall();
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), kTestAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), kTestAccessRights);
}

TEST_F(VideowallItemAccessResolverTest, basicNotificationSignals)
{
    // Add videowall.
    const auto videowall = addVideoWall();
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Change videowall items.
    const auto layout = addLayoutForVideoWall(videowall);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Change videowall access rights.
    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), AccessRights()}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Remove.
    manager->removeSubjects({kTestSubjectId});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Clear.
    manager->clear();
    NX_ASSERT_SIGNAL(resourceAccessReset);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(VideowallItemAccessResolverTest, layoutAccessViaVideowall)
{
    auto videowall = addVideoWall();
    auto target = addLayoutForVideoWall(videowall);

    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), kTestAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, target),
        videowallItemAccessRights(kTestAccessRights));
}

TEST_F(VideowallItemAccessResolverTest, layoutAddedBeforeVideowall)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = createVideoWall();

    // What's going on on drop.
    auto layout = addLayoutForVideoWall(videowall);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    resourcePool()->addResource(videowall);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, layoutAddedBeforeVideowallItem)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    auto layout = addLayout();
    NX_ASSERT_TEST_SUBJECT_CHANGED();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    layout->setParentId(videowall->getId());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, layoutAddedAfterVideowallItem)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto layout = createLayout();
    layout->setParentId(videowall->getId());

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);

    resourcePool()->addResource(layout);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));
    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, layoutRemovedFromVideowallItems)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto layout = addLayoutForVideoWall(videowall);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    QnVideoWallItem item = *videowall->items()->getItems().cbegin();
    item.layout = {};
    videowall->items()->updateItem(item);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, layoutRemovedFromThePoolFirst)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto layout = addLayoutForVideoWall(videowall);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    resourcePool()->removeResource(layout);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    QnVideoWallItem item = *videowall->items()->getItems().cbegin();
    item.layout = {};
    videowall->items()->updateItem(item);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(VideowallItemAccessResolverTest, videowallLayoutBecomesParentless)
// Not a standard scenario, but should work nonetheless.
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto layout = addLayoutForVideoWall(videowall);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    layout->setParentId({});

    // No longer gains access rights through the videowall.
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());
    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, dynamicAccessRightsChange)
{
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), kTestAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    const auto testAccessRights2 = AccessRights{};
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, testAccessRights2}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), testAccessRights2);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(testAccessRights2));
    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, videowallAdded)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = createVideoWall();
    auto layout = addLayoutForVideoWall(videowall);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    resourcePool()->addResource(videowall);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), kTestAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    // The signal is not sent for gaining access to the videowall when it's added to the pool.
    // But it is sent for gaining access to its layout.
    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, videowallRemoved)
{
    manager->setOwnResourceAccessMap(kTestSubjectId, {{kAllVideoWallsGroupId, kTestAccessRights}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto videowall = addVideoWall();
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    auto layout = addLayoutForVideoWall(videowall);
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), kTestAccessRights);
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout),
        videowallItemAccessRights(kTestAccessRights));

    resourcePool()->removeResource(videowall);

    ASSERT_EQ(resolver->accessRights(kTestSubjectId, videowall), AccessRights());
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, layout), AccessRights());

    // The signal is not sent for losing access to the videowall when it's removed from the pool.
    // But it is sent for losing access to its layout.
    NX_ASSERT_TEST_SUBJECT_CHANGED();
}

TEST_F(VideowallItemAccessResolverTest, accessDetails)
{
    auto videowall = addVideoWall();
    videowall->setName("Videowall");
    auto layout = addLayoutForVideoWall(videowall);
    layout->setName("Layout");

    manager->setOwnResourceAccessMap(kTestSubjectId, {{videowall->getId(), kTestAccessRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, videowall, AccessRight::edit),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, videowall, AccessRight::manageBookmarks),
        ResourceAccessDetails());

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::manageBookmarks),
        ResourceAccessDetails());

    // Direct access rights given to a videowall layout should be ignored.

    manager->setOwnResourceAccessMap(kTestSubjectId, {
        {videowall->getId(), kTestAccessRights},
        {layout->getId(), kTestAccessRights | AccessRight::manageBookmarks}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {videowall}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, layout, AccessRight::manageBookmarks),
        ResourceAccessDetails());
}

} // namespace test
} // namespace nx::core::access
