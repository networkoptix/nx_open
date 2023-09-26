// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/intercom/utils.h>

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;
using namespace nx::vms::api;

// String constants.
static constexpr auto kUniqueLayoutName = "unique_layout_name";

// Predefined conditions.
static const auto kUniqueLayoutNameCondition = displayFullMatch(kUniqueLayoutName);

TEST_F(ResourceTreeModelTest, layoutAdds)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When layout resource with certain unique name is added to the resource pool.
    const auto camera = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, layoutRemoves)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueLayoutNameCondition));

    // When previously added layout is removed from resource pool.
    resourcePool()->removeResource(layout);

    // Then no more nodes with corresponding display text found in the Resource Tree.
    ASSERT_TRUE(noneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, layoutIconType)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node have layout icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::Layout)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIconType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When non-owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node have shared layout icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::SharedLayout)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, lockedLayoutIconType)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    const auto regularLayoutIconCondition = allOf(
        iconStatusMatch(QnResourceIconCache::Unknown), iconTypeMatch(QnResourceIconCache::Layout));

    // After layout locking...
    layout->setLocked(true);

    // That node have locked layout extra status icon.
    ASSERT_TRUE(regularLayoutIconCondition(layoutIndex));
    ASSERT_TRUE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::locked)(layoutIndex));

    // After unlocking it back...
    layout->setLocked(false);

    // That node displays plain layout icon again.
    ASSERT_TRUE(regularLayoutIconCondition(layoutIndex));
    ASSERT_FALSE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::locked)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutIsChildOfLayoutsNode)
{
    // When user is logged in.
    const auto powerUser = loginAsPowerUser("power_user");

    // When layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, powerUser->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And this node is child of "Layouts" node.
    ASSERT_TRUE(directChildOf(layoutsNodeCondition())(layoutIndex));
}

TEST_F(ResourceTreeModelTest, ownLayoutIsEditableByAdmin)
{
    // When user with power user permissions is logged in.
    const auto powerUser = loginAsPowerUser("power_user");

    // When layout with unique name owned by user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, powerUser->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIsEditableByAdmin)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When shared layout with unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, QnUuid());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIsNotEditableByNonAdmin)
{
    // When user without power user permissions is logged in.
    const auto liveViewerUser = loginAsLiveViewer("live_viever");

    // When shared layout with unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, QnUuid());

    // And access to the shared layout is granted to the logged in user.
    setupAccessToResourceForUser(liveViewerUser, layout, true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it hasn't editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, ownLayoutIsEditableByNonAdmin)
{
    // When user without power user permissions is logged in.
    const auto liveViewerUser = loginAsLiveViewer("live_viever");

    // When layout with unique name owned by mentioned user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, liveViewerUser->getId());

    // Then exactly one node with corresponding display texts appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutHelpTopic)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::MainWindow_Tree_Layouts)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutProvidesResource)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node provides pointer to the layout resource.
    ASSERT_TRUE(dataMatch(Qn::ResourceRole, QVariant::fromValue<QnResourcePtr>(layout))(
        layoutIndex));
}

TEST_F(ResourceTreeModelTest, DISABLED_layoutModificationMark)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // When that layout marked as changed in snapshot manager.
    layoutSnapshotManager()->markChanged(layout->getId(), true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    // And that node have modification mark "*".
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayStartsWith(kUniqueLayoutName),
            displayEndsWith("*"))));
}

TEST_F(ResourceTreeModelTest, layoutNodeIsDragEnabled)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, dontShowServiceLayoutsWhileInLayoutSearchMode)
{
    // When video wall is added to the resource pool.
    const auto videoWall = addVideoWall("video_wall");

    // When video wall item is added to the video wall.
    addVideoWallScreen("video_wall_screen", videoWall);

    // When video wall matrix with certain unique name is added to the video wall.
    addVideoWallMatrix("video_wall_matrix", videoWall);

    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When "Layouts" search filter is selected.
    setFilterMode(entity_resource_tree::ResourceTreeComposer::FilterMode::allLayoutsFilter);

    // Then there are still no resources anywhere in the resource tree.
    ASSERT_TRUE(noneMatches(nodeTypeDataMatch(NodeType::resource)));
}

TEST_F(ResourceTreeModelTest, intercomLayoutNodeVisibleUnderAdmin)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user is logged in.
    const auto powerUser = loginAsPowerUser("power_user");

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, powerUser->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-power user is added to the resource pool.
    const auto otherUser = addUser(kUniqueUserName, api::kAdvancedViewersGroupId);

    // When intercom is not accessible for non-power user.
    // Then a single copy of intercom layout appears in the resource tree.
    ASSERT_EQ(allMatchingIndexes(kUniqueLayoutNameCondition).size(), 1);

    // When intercom is accessible for non-power user.
    setupAccessToResourceForUser(otherUser, intercom, true);

    // Then still a single copy of intercom layout appears in the resource tree.
    ASSERT_EQ(allMatchingIndexes(kUniqueLayoutNameCondition).size(), 1);
}

TEST_F(ResourceTreeModelTest, intercomLayoutNodeInvisibleUnderUserWithNotEnoughPermissions)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user with unique name and power user permissions is added to the resource pool.
    const auto powerUser = addUser("power_user", api::kPowerUsersGroupId);

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, powerUser->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-power user is logged in.
    const auto otherUser = loginAsCustomUser(kUniqueUserName);

    // When intercom is accessible for non-power user as view-only camera.
    setupAccessToResourceForUser(otherUser, intercom, AccessRight::view);

    // Then no nodes with corresponding layout display text appears in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, intercomLayoutNodeVisibleUnderUserWithPermissions)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user with unique name and power user permissions is added to the resource pool.
    const auto powerUser = addUser("power_user", api::kPowerUsersGroupId);

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, powerUser->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-power user is logged in.
    const auto otherUser = loginAsCustomUser(kUniqueUserName);

    // When intercom is accessible for non-power user for full operation.
    setupAccessToResourceForUser(otherUser, intercom, AccessRight::view | AccessRight::userInput);

    // Then exactly one node with corresponding layout display text appears in the resource tree.
    ASSERT_TRUE(uniqueMatchingIndex(kUniqueLayoutNameCondition).isValid());
}

TEST_F(ResourceTreeModelTest, intercomLayoutNodeNotVisibleUnderUser)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user with unique name and power user permissions is added to the resource pool.
    const auto powerUser = addUser("power_user", api::kPowerUsersGroupId);

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, powerUser->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-power user is logged in.
    const auto otherUser = loginAsCustomUser(kUniqueUserName);

    // And intercom node doesn't appear in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kIntercomCameraName)));

    // Then layout node doesn't appear in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, intercomLayoutLayoutTypeCheck)
{
    // String constants
    static constexpr auto kUserName = "user_name";
    static constexpr auto kVideowallName = "videowall_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user is logged in.
    const auto powerUser = loginAsPowerUser("power_user");

    // When layout is created.
    const auto layout = addIntercomLayout(kUniqueLayoutName);

    // When user is added to the resource pool.
    auto user = addUser(kUserName, api::kPowerUsersGroupId);

    // When user is the layout parent.
    layout->setParentId(user->getId());

    // Layout is not an intercom layout.
    ASSERT_TRUE(!nx::vms::common::isIntercomLayout(layout));
    ASSERT_NE(layout->layoutType(), LayoutResource::LayoutType::intercom);

    // When videowall is added to the resource pool.
    auto videowall = addVideoWall(kVideowallName);

    // When videowall is the layout parent.
    layout->setParentId(videowall->getId());

    // Layout is not an intercom layout.
    ASSERT_TRUE(!nx::vms::common::isIntercomLayout(layout));
    ASSERT_NE(layout->layoutType(), LayoutResource::LayoutType::intercom);

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, powerUser->getId());

    // When intercom is the layout parent.
    layout->setParentId(intercom->getId());

    // Layout is an intercom layout.
    ASSERT_TRUE(nx::vms::common::isIntercomLayout(layout));
    ASSERT_EQ(layout->layoutType(), LayoutResource::LayoutType::intercom);
}

} // namespace test
} // namespace nx::vms::client::desktop
