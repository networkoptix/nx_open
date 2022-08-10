// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resources/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/intercom/utils.h>
#include <ui/help/help_topics.h>

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueLayoutName = "unique_layout_name";

// Predefined conditions.
static const auto kUniqueLayoutNameCondition = displayFullMatch(kUniqueLayoutName);

TEST_F(ResourceTreeModelTest, layoutAdds)
{
    // When user is logged in.
    const auto user = loginAsAdmin("admin");

    // When layout resource with certain unique name is added to the resource pool.
    const auto camera = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, layoutRemoves)
{
    // When user is logged in.
    const auto user = loginAsAdmin("admin");

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
    const auto user = loginAsAdmin("admin");

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
    loginAsAdmin("admin");

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
    const auto user = loginAsAdmin("admin");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    const auto lockedLayoutIconCondition = allOf(
        iconStatusMatch(QnResourceIconCache::Locked), iconTypeMatch(QnResourceIconCache::Layout));

    const auto regularLayoutIconCondition = allOf(
        iconStatusMatch(QnResourceIconCache::Unknown), iconTypeMatch(QnResourceIconCache::Layout));

    // After layout locking...
    layout->setLocked(true);

    // That node have locked layout icon.
    ASSERT_TRUE(lockedLayoutIconCondition(layoutIndex));

    // After unlocking it back...
    layout->setLocked(false);

    // That node displays plain layout icon again.
    ASSERT_TRUE(regularLayoutIconCondition(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutIsChildOfLayoutsNode)
{
    // When user is logged in.
    const auto adminUser = loginAsAdmin("admin");

    // When layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, adminUser->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And this node is child of "Layouts" node.
    ASSERT_TRUE(directChildOf(layoutsNodeCondition())(layoutIndex));
}

TEST_F(ResourceTreeModelTest, ownLayoutIsEditableByAdmin)
{
    // When user with administrator permissions is logged in.
    const auto adminUser = loginAsAdmin("admin");

    // When layout with unique name owned by user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, adminUser->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIsEditableByAdmin)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When shared layout with unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, QnUuid());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, otherUserLayoutIsEditableByAdmin)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When non-admin user is added to the resource pool.
    const auto liveViewerUser =
        addUser("live_viever", nx::vms::api::GlobalPermission::liveViewerPermissions);

    // When layout with unique name owned by non-admin user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, liveViewerUser->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIsNotEditableByNonAdmin)
{
    // When user without administrator permissions is logged in.
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
    // When user without administrator permissions is logged in.
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
    const auto user = loginAsAdmin("admin");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, Qn::MainWindow_Tree_Layouts_Help)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutProvidesResource)
{
    // When user is logged in.
    const auto user = loginAsAdmin("admin");

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
    const auto user = loginAsAdmin("admin");

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
    const auto user = loginAsAdmin("admin");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, anotherUserLayoutShownUnderUserNode)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";

    // When user with unique name and administrator permissions is added to the resource pool.
    const auto adminUser = addUser(kUniqueUserName, GlobalPermission::adminPermissions);

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, adminUser->getId());

    // When administrator user is logged in (not layout owner).
    loginAsAdmin("admin");

    // Then exactly one node with corresponding layout display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And this node is child of user node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueUserName))(layoutIndex));
}

TEST_F(ResourceTreeModelTest, anotherUserLayoutAppearsUnderUserNodeWithinUserSession)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";

    // When user with unique name and administrator permissions is added to the resource pool.
    const auto adminUser = addUser(kUniqueUserName, GlobalPermission::adminPermissions);

    // When administrator user is logged in.
    loginAsAdmin("admin");

    // When layout with certain unique name owned by another user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, adminUser->getId());

    // Then exactly one node with corresponding layout display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And this node is child of user node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueUserName))(layoutIndex));
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
    const auto user = loginAsAdmin("admin");

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
    const auto admin = loginAsAdmin("admin");

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, admin->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-admin user is added to the resource pool.
    const auto otherUser = addUser(kUniqueUserName, GlobalPermission::userInput);

    // When intercom is accessible for non-admin user.
    setupAccessToResourceForUser(otherUser, intercom, true);

    // Then two copies of intercom layout appeard in the resource tree.
    ASSERT_EQ(allMatchingIndexes(kUniqueLayoutNameCondition).size(), 2);

    // When intercom is not accessible for non-admin user.
    setupAccessToResourceForUser(otherUser, intercom, false);

    // Then only single copy of intercom layout appeard in the resource tree.
    ASSERT_EQ(allMatchingIndexes(kUniqueLayoutNameCondition).size(), 1);
}

TEST_F(ResourceTreeModelTest, intercomLayoutNodeVisibleUnderUser)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user with unique name and administrator permissions is added to the resource pool.
    const auto admin = addUser("admin", GlobalPermission::adminPermissions);

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, admin->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-admin user is logged in.
    const auto otherUser = loginAsCustomUser(kUniqueUserName);

    // When intercom is accessible for non-admin user.
    setupAccessToResourceForUser(otherUser, intercom, true);

    // Then exactly one node with corresponding layout display text appears in the resource tree.
    ASSERT_TRUE(uniqueMatchingIndex(kUniqueLayoutNameCondition).isValid());
}

TEST_F(ResourceTreeModelTest, intercomLayoutNodeNotVisibleUnderUser)
{
    // String constants
    static constexpr auto kUniqueUserName = "unique_user_name";
    static constexpr auto kIntercomCameraName = "intercom_camera_name";

    // When user with unique name and administrator permissions is added to the resource pool.
    const auto admin = addUser("admin", GlobalPermission::adminPermissions);

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, admin->getId());

    // When intercom layout is added to the resource pool.
    const auto layout = addIntercomLayout(kUniqueLayoutName, intercom->getId());

    // When non-admin user is logged in.
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
    const auto admin = loginAsAdmin("admin");

    // When layout is created.
    const auto layout = addIntercomLayout(kUniqueLayoutName);

    // When user is added to the resource pool.
    auto user = addUser(kUserName, nx::vms::api::GlobalPermission::adminPermissions);

    // When user is the layout parent.
    layout->setParentId(user->getId());

    // Layout is not an intercom layout.
    ASSERT_TRUE(!nx::vms::common::isIntercomLayout(layout));

    // When videowall is added to the resource pool.
    auto videowall = addVideoWall(kVideowallName);

    // When videowall is the layout parent.
    layout->setParentId(videowall->getId());

    // Layout is not an intercom layout.
    ASSERT_TRUE(!nx::vms::common::isIntercomLayout(layout));

    // When intercom camera is added to the resource pool.
    const auto intercom = addIntercomCamera(kIntercomCameraName, admin->getId());

    // When intercom is the layout parent.
    layout->setParentId(intercom->getId());

    // Layout is an intercom layout.
    ASSERT_TRUE(nx::vms::common::isIntercomLayout(layout));
}

} // namespace test
} // namespace nx::vms::client::desktop
