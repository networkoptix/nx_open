#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <ui/style/resource_icon_cache.h>
#include <api/runtime_info_manager.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

TEST_F(ResourceTreeModelTest, videoWallAdds)
{
    // String constants.
    static constexpr auto kVideoWallName = "unique_video_wall_name";

    // Set up environment.
    loginAsAdmin("admin");
    addVideoWall(kVideoWallName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kVideoWallName)));
}

TEST_F(ResourceTreeModelTest, videoWallRemoves)
{
    // String constants.
    static constexpr auto kVideoWallName = "unique_video_wall_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto videoWall = addVideoWall(kVideoWallName);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kVideoWallName)));

    resourcePool()->removeResource(videoWall);
    ASSERT_TRUE(noneMatches(displayFullMatch(kVideoWallName)));
}

TEST_F(ResourceTreeModelTest, videoWallIcon)
{
    // String constants.
    static constexpr auto kVideoWallName = "unique_video_wall_name";

    // Set up environment.
    loginAsAdmin("admin");
    addVideoWall(kVideoWallName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kVideoWallName),
            iconTypeMatch(QnResourceIconCache::VideoWall))));
}

TEST_F(ResourceTreeModelTest, videoWallChildrenIcons)
{
    // String constants.
    static constexpr auto kVideoWallName = "unique_video_wall_name";
    static constexpr auto kVideoWallItemName = "unique_video_wall_item_name";
    static constexpr auto kVideoWallMatrixName = "unique_video_wall_matrix_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto videoWall = addVideoWall(kVideoWallName);
    addVideoWallItem(kVideoWallItemName, videoWall);
    addVideoWallMatrix(kVideoWallMatrixName, videoWall);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(videoWallNodeCondition()),
            displayFullMatch(kVideoWallItemName),
            iconTypeMatch(QnResourceIconCache::VideoWallItem))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(videoWallNodeCondition()),
            displayFullMatch(kVideoWallMatrixName),
            iconTypeMatch(QnResourceIconCache::VideoWallMatrix))));
}

TEST_F(ResourceTreeModelTest, videoWallsAreTopLevelNodes)
{
    // String constants.
    static constexpr auto kVideoWallName1 = "unique_video_wall_name_1";
    static constexpr auto kVideoWallName2 = "unique_video_wall_name_2";

    // Set up environment.
    loginAsAdmin("admin");
    addVideoWall(kVideoWallName1);
    addVideoWall(kVideoWallName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            topLevelNode(),
            displayFullMatch(kVideoWallName1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            topLevelNode(),
            displayFullMatch(kVideoWallName2))));
}

TEST_F(ResourceTreeModelTest, videoWallModificationMark)
{
    // String constants.
    static constexpr auto kVideoWallName = "unique_video_wall_name";
    static constexpr auto kVideoWallItemName = "unique_video_wall_item_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto videoWall = addVideoWall(kVideoWallName);
    addVideoWallItem(kVideoWallItemName, videoWall);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kVideoWallName)));

    auto videoWallItemsLayouts = resourcePool()->getResourcesByParentId(videoWall->getId());
    ASSERT_FALSE(videoWallItemsLayouts.isEmpty());

    layoutSnapshotManager()->markChanged(videoWallItemsLayouts.first()->getId(), true);
    ASSERT_TRUE(noneMatches(displayFullMatch(kVideoWallName)));
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayStartsWith(kVideoWallName),
            displayEndsWith("*"))));
}
