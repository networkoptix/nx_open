#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <ui/style/resource_icon_cache.h>
#include <api/runtime_info_manager.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::index_condition;

TEST_F(ResourceTreeModelTest, videoWallAdds)
{
    // String constants.
    static constexpr auto videoWallName = "unique_video_wall_name";

    // Set up environment.
    loginAsAdmin("admin");
    addVideoWall(videoWallName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(videoWallName)));
}

TEST_F(ResourceTreeModelTest, videoWallRemoves)
{
    // String constants.
    static constexpr auto videoWallName = "unique_video_wall_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto videoWall = addVideoWall(videoWallName);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(videoWallName)));

    resourcePool()->removeResource(videoWall);
    ASSERT_TRUE(noneMatches(displayFullMatch(videoWallName)));
}

TEST_F(ResourceTreeModelTest, videoWallIcon)
{
    // String constants.
    static constexpr auto videoWallName = "unique_video_wall_name";

    // Set up environment.
    loginAsAdmin("admin");
    addVideoWall(videoWallName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(videoWallName),
            iconTypeMatch(QnResourceIconCache::VideoWall))));
}

TEST_F(ResourceTreeModelTest, videoWallChildrenIcons)
{
    // String constants.
    static constexpr auto videoWallName = "unique_video_wall_name";
    static constexpr auto videoWallItemName = "unique_video_wall_item_name";
    static constexpr auto videoWallMatrixName = "unique_video_wall_matrix_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto videoWall = addVideoWall(videoWallName);
    addVideoWallItem(videoWallItemName, videoWall);
    addVideoWallMatrix(videoWallMatrixName, videoWall);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(videoWallNodeCondition()),
            displayFullMatch(videoWallItemName),
            iconTypeMatch(QnResourceIconCache::VideoWallItem))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(videoWallNodeCondition()),
            displayFullMatch(videoWallMatrixName),
            iconTypeMatch(QnResourceIconCache::VideoWallMatrix))));
}

TEST_F(ResourceTreeModelTest, videoWallsAreTopLevelNodes)
{
    // String constants.
    static constexpr auto videoWallName1 = "unique_video_wall_name_1";
    static constexpr auto videoWallName2 = "unique_video_wall_name_2";

    // Set up environment.
    loginAsAdmin("admin");
    addVideoWall(videoWallName1);
    addVideoWall(videoWallName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            topLevelNode(),
            displayFullMatch(videoWallName1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            topLevelNode(),
            displayFullMatch(videoWallName2))));
}

TEST_F(ResourceTreeModelTest, videoWallModificationMark)
{
    // String constants.
    static constexpr auto videoWallName = "unique_video_wall_name";
    static constexpr auto videoWallItemName = "unique_video_wall_item_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto videoWall = addVideoWall(videoWallName);
    addVideoWallItem(videoWallItemName, videoWall);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(videoWallName)));

    auto videoWallItemsLayouts = resourcePool()->getResourcesByParentId(videoWall->getId());
    ASSERT_FALSE(videoWallItemsLayouts.isEmpty());

    layoutSnapshotManager()->markChanged(videoWallItemsLayouts.first()->getId(), true);
    ASSERT_TRUE(noneMatches(displayFullMatch(videoWallName)));
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayStartsWith(videoWallName),
            displayEndsWith("*"))));
}
