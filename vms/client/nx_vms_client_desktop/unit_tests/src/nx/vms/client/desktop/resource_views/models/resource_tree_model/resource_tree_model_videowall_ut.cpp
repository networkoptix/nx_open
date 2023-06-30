// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <api/runtime_info_manager.h>
#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueVideoWallName = "unique_video_wall_name";
static constexpr auto kUniqueVideoWallScreenName = "unique_video_wall_item_name";
static constexpr auto kUniqueVideoWallMatrixName = "unique_video_wall_matrix_name";

// Predefined conditions.
static const auto kUniqueVideoWallNameCondition = displayFullMatch(kUniqueVideoWallName);
static const auto kUniqueVideoWallScreenNameCondition =
    displayFullMatch(kUniqueVideoWallScreenName);
static const auto kUniqueVideoWallMatrixNameCondition =
    displayFullMatch(kUniqueVideoWallMatrixName);

TEST_F(ResourceTreeModelTest, videoWallAdds)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallNameCondition));
}

TEST_F(ResourceTreeModelTest, videoWallScreenAdds)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallScreenNameCondition));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixAdds)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallMatrixNameCondition));
}

TEST_F(ResourceTreeModelTest, videoWallRemoves)
{
    // When video wall with certain unique name is added to the resource pool.
    const auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallNameCondition));

    // When previously added video wall is removed from resource pool.
    resourcePool()->removeResource(videoWall);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueVideoWallNameCondition));
}

TEST_F(ResourceTreeModelTest, videoWallScreenRemoves)
{
    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    const auto videoWallScreen = addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallScreenNameCondition));

    // When previously added video wall screen is removed from video wall.
    videoWall->items()->removeItem(videoWallScreen.uuid);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueVideoWallScreenNameCondition));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixRemoves)
{
    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    const auto videoWallMatrix = addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallMatrixNameCondition));

    // When previously added video wall matrix is removed from video wall.
    videoWall->matrices()->removeItem(videoWallMatrix.uuid);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueVideoWallMatrixNameCondition));
}

TEST_F(ResourceTreeModelTest, videoWallIconType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And it have videowall icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::VideoWall)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenIconType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // And it have videowall item icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::VideoWallItem)(videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixIconType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // And it have videowall matrix item icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::VideoWallMatrix)(videoWallMatrixIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenIconStatus)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    const auto videoWallScreen = addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // When video wall screen is offline.
    setVideoWallScreenRuntimeStatus(videoWall, videoWallScreen, false);

    // Then icon has Offline decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Offline)
        (uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition)));

    // When video wall screen is online and have null "controlled by" UUID.
    setVideoWallScreenRuntimeStatus(videoWall, videoWallScreen, true);

    // Then icon has no decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Unknown)
        (uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition)));

    // When video wall screen is online and have "controlled by" UUID same as common module GUID.
    setVideoWallScreenRuntimeStatus(videoWall, videoWallScreen, true, commonModule()->peerId());

    // Then icon has Control decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Control)
        (uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition)));

    // When video wall screen is online and have "controlled by" UUID not same as common module GUID.
    setVideoWallScreenRuntimeStatus(videoWall, videoWallScreen, true, QnUuid::createUuid());

    // Then icon has Locked decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Locked)
        (uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition)));
}

TEST_F(ResourceTreeModelTest, videoWallsAreTopLevelNodes)
{
    // String constants.
    static constexpr auto kUniqueVideoWallName1 = "unique_video_wall_name_1";
    static constexpr auto kUniqueVideoWallName2 = "unique_video_wall_name_2";

    // When user is logged in.
    loginAsPowerUser("power_user");

    // When two video wall with different unique names are added to the resource pool.
    addVideoWall(kUniqueVideoWallName1);
    addVideoWall(kUniqueVideoWallName2);

    // Then exactly two nodes with corresponding display texts appear in the resource tree.
    const auto videoWallIndex1 = uniqueMatchingIndex(displayFullMatch(kUniqueVideoWallName1));
    const auto videoWallIndex2 = uniqueMatchingIndex(displayFullMatch(kUniqueVideoWallName2));

    // And they are top-level nodes.
    ASSERT_TRUE(topLevelNode()(videoWallIndex1));
    ASSERT_TRUE(topLevelNode()(videoWallIndex2));
}

TEST_F(ResourceTreeModelTest, videoWallScreenIsChildOfVideoWall)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    const auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // And its parent node display text matches video wall name.
    ASSERT_TRUE(directChildOf(kUniqueVideoWallNameCondition)(videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenSingleItemRename)
{
    // String constants.
    static constexpr auto kUniqueVideoWallScreenName1 = "unique_video_wall_screen_name_1";
    static constexpr auto kUniqueVideoWallScreenName2 = "unique_video_wall_screen_name_2";
    static constexpr auto kRenamedVideoWallScreenName = "renamed_video_wall_screen_name";

    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    const auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When two screens with different names are added to the video wall.
    auto screen1 = addVideoWallScreen(kUniqueVideoWallScreenName1, videoWall);
    auto screen2 = addVideoWallScreen(kUniqueVideoWallScreenName2, videoWall);

    // Then there is single occurrence of node with each name found in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueVideoWallScreenName1)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueVideoWallScreenName2)));

    // When new name is set to the one of screens.
    screen1.name = kRenamedVideoWallScreenName;
    updateVideoWallScreen(videoWall, screen1);

    // Then one of the screen nodes in the resource tree gets new name while second screen node
    // remain unchanged.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueVideoWallScreenName1)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kRenamedVideoWallScreenName)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueVideoWallScreenName2)));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixIsChildOfVideoWall)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    const auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // And its parent node display text matches video wall name.
    ASSERT_TRUE(directChildOf(kUniqueVideoWallNameCondition)(videoWallMatrixIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixSingleItemRename)
{
    // String constants.
    static constexpr auto kUniqueVideoWallMatrixName1 = "unique_video_wall_matrix_name_1";
    static constexpr auto kUniqueVideoWallMatrixName2 = "unique_video_wall_matrix_name_2";
    static constexpr auto kRenamedVideoWallMatrixName = "renamed_video_wall_matrix_name";

    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    const auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When two screens with different names are added to the video wall.
    auto matrix1 = addVideoWallMatrix(kUniqueVideoWallMatrixName1, videoWall);
    auto matrix2 = addVideoWallMatrix(kUniqueVideoWallMatrixName2, videoWall);

    // Then there is single occurrence of node with each name found in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueVideoWallMatrixName1)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueVideoWallMatrixName2)));

    // When new name is set to the one of screens.
    matrix1.name = kRenamedVideoWallMatrixName;
    updateVideoWallMatrix(videoWall, matrix1);

    // Then one of the screen nodes in the resource tree gets new name while second screen node
    // remain unchanged.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueVideoWallMatrixName1)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kRenamedVideoWallMatrixName)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueVideoWallMatrixName2)));
}

TEST_F(ResourceTreeModelTest, videoWallIsNotDisplayedIfNotLoggedIn)
{
    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueVideoWallNameCondition));

    // When no user is logged in.
    logout();

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueVideoWallNameCondition));

    // Then no more nodes with with videowall icon type found in the resource tree.
    ASSERT_TRUE(noneMatches(iconTypeMatch(QnResourceIconCache::VideoWall)));
}

TEST_F(ResourceTreeModelTest, videoWallHelpTopic)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::Videowall)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenHelpTopic)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::Videowall_Display)(
        videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixHelpTopic)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::Videowall_Management)(
        videoWallMatrixIndex));
}

TEST_F(ResourceTreeModelTest, videoWallTooltip)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node have same tooltip text as display text.
    ASSERT_TRUE(dataMatch(Qt::ToolTipRole, kUniqueVideoWallName)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallDisplayNameMapping)
{
    // When video wall with certain unique name is added to the resource pool.
    auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node have same display text as QnVideoWallResource::getName() returns.
    ASSERT_EQ(videoWallIndex.data().toString(), videoWall->getName());
}

TEST_F(ResourceTreeModelTest, videoWallNodeProvidesResource)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    const auto videoWall = addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node provides pointer to the video wall.
    ASSERT_TRUE(dataMatch(core::ResourceRole, QVariant::fromValue<QnResourcePtr>(videoWall))(
        videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenNodeProvidesUuid)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    const auto videoWallScreen = addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // TODO: #vbreus Remove redundant data role.
    // And that node provides video wall Item UUID by both core::UuidRole and ItemUuidRole roles.
    ASSERT_TRUE(dataMatch(core::UuidRole, QVariant::fromValue<QnUuid>(videoWallScreen.uuid))(
        videoWallScreenIndex));
    ASSERT_TRUE(dataMatch(Qn::ItemUuidRole, QVariant::fromValue<QnUuid>(videoWallScreen.uuid))(
        videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixNodeProvidesUuid)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    const auto videoWallMatrix = addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // TODO: #vbreus Remove redundant data role.
    // And that node provides video wall Matrix UUID by both core::UuidRole and ItemUuidRole roles.
    ASSERT_TRUE(dataMatch(core::UuidRole, QVariant::fromValue<QnUuid>(videoWallMatrix.uuid))(
        videoWallMatrixIndex));
    ASSERT_TRUE(dataMatch(Qn::ItemUuidRole, QVariant::fromValue<QnUuid>(videoWallMatrix.uuid))(
        videoWallMatrixIndex));
}

TEST_F(ResourceTreeModelTest, videoWallNodeIsVisibleAndEditableOnlyByUsersHaveSuchPermissions)
{
    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node is can be edited.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(videoWallIndex));

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then no video walls found in the resource tree.
    ASSERT_TRUE(noneMatches(iconTypeMatch(QnResourceIconCache::VideoWall)));

    // When custom user with videowall control is logged in.
    loginAsCustomUser("customUser");
    setupControlAllVideoWallsAccess(currentUser());

    // Then exactly one node with corresponding display text appears in the resource tree.
    videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node can be edited.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, DISABLED_videoWallModificationMark)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    auto videoWall = addVideoWall(kUniqueVideoWallName);

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // When some video wall screen layout is marked as changed.
    auto videoWallScreensLayouts = resourcePool()->getResourcesByParentId(videoWall->getId());
    ASSERT_FALSE(videoWallScreensLayouts.isEmpty());
    layoutSnapshotManager()->markChanged(videoWallScreensLayouts.first()->getId(), true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    // And that node have modification mark "*".
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayStartsWith(kUniqueVideoWallName),
            displayEndsWith("*"))));
}

TEST_F(ResourceTreeModelTest, videoWallNodeIsDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenNodeIsDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixNodeIsNotDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // And that node isn't draggable.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDragEnabled)(videoWallMatrixIndex));
}

TEST_F(ResourceTreeModelTest, videoWallHasNotItemNeverHasChildrenFlag)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node is declared as node which may have children.
    ASSERT_FALSE(hasFlag(Qt::ItemNeverHasChildren)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenHasItemNeverHasChildrenFlag)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // And that node is declared as leaf node.
    ASSERT_TRUE(hasFlag(Qt::ItemNeverHasChildren)(videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixHasItemNeverHasChildrenFlag)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // And that node is declared as leaf node.
    ASSERT_TRUE(hasFlag(Qt::ItemNeverHasChildren)(videoWallMatrixIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenIsDropEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall with certain unique name is added to the resource pool.
    addVideoWall(kUniqueVideoWallName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallIndex = uniqueMatchingIndex(kUniqueVideoWallNameCondition);

    // And that node is accept drop action.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDropEnabled)(videoWallIndex));
}

TEST_F(ResourceTreeModelTest, videoWallScreenNodeType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall screen with certain unique name is added to the video wall.
    addVideoWallScreen(kUniqueVideoWallScreenName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallScreenIndex = uniqueMatchingIndex(kUniqueVideoWallScreenNameCondition);

    // And that index provides certain node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::videoWallItem)(videoWallScreenIndex));
}

TEST_F(ResourceTreeModelTest, videoWallMatrixNodeType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("Video Wall");

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix(kUniqueVideoWallMatrixName, videoWall);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto videoWallMatrixIndex = uniqueMatchingIndex(kUniqueVideoWallMatrixNameCondition);

    // And that index provides certain node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::videoWallMatrix)(videoWallMatrixIndex));
}

} // namespace test
} // namespace nx::vms::client::desktop
