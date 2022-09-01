// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueUserName = "unique_user_name";
static constexpr auto kUniqueCameraName = "unique_camera_name";
static constexpr auto kUniqueLayoutName = "unique_layout_name";
static constexpr auto kUniqueSharedLayoutName = "unique_shared_layout_name";

// Predefined conditions.
static const auto kUniqueUserNameCondition = displayFullMatch(kUniqueUserName);
static const auto kUniqueCameraNameCondition = displayFullMatch(kUniqueCameraName);
static const auto kUniqueLayoutNameCondition = displayFullMatch(kUniqueLayoutName);
static const auto kUniqueSharedLayoutNameCondition = displayFullMatch(kUniqueSharedLayoutName);

TEST_F(ResourceTreeModelTest, viewerUserAdds)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When user with viewer permissions and unique name is added to the resource pool.
    addUser(kUniqueUserName, GlobalPermission::viewerPermissions);

    // Then exactly one node with corresponding user display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueUserNameCondition));
}

TEST_F(ResourceTreeModelTest, viewerUserRemoves)
{
    // When user with viewer permissions and unique name is added to the resource pool.
    const auto user = addUser(kUniqueUserName, GlobalPermission::viewerPermissions);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with unique user display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueUserNameCondition));

    // When previously added user is removed from resource pool.
    resourcePool()->removeResource(user);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueUserNameCondition));
}

TEST_F(ResourceTreeModelTest, disabledUserIsHidden)
{
    // When user with viewer permissions and unique name is added to the resource pool.
    const auto user = addUser(kUniqueUserName, GlobalPermission::viewerPermissions);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with unique user display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueUserNameCondition));

    // When previously added user is made disabled.
    user->setEnabled(false);

    // Then no more nodes with unique user display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueUserNameCondition));

    // When previously disabled user is made enabled again.
    user->setEnabled(true);

    // Then exactly one node with unique user display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueUserNameCondition));
}

TEST_F(ResourceTreeModelTest, sharedDevicesAppearWithinGroupForCustomUser)
{
    // When custom user with unique name is added to the resource pool.
    const auto customUser = addUser(kUniqueUserName, GlobalPermission::customUser);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then there are no custom user's shared resources groups found in the resource tree.
    ASSERT_TRUE(noneMatches(sharedResourcesNodeCondition()));

    // When camera is shared to the custom user.
    setupAccessToResourceForUser(customUser, camera, true);

    // Then exactly one node with corresponding camera display text appears as child of user's
    // shared resources group.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            kUniqueCameraNameCondition,
            directChildOf(sharedResourcesNodeCondition()))));
}

TEST_F(ResourceTreeModelTest, sharedDevicesGroupDisappearsWhenNoSharedResourcesLeftForCustomUser)
{
    // When custom user with unique user name is added to the resource pool.
    const auto customUser = addUser(kUniqueUserName, GlobalPermission::customUser);

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with unique camera name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When camera is shared to the custom user.
    setupAccessToResourceForUser(customUser, camera, true);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one user's shared resources group node found in the resource tree.
    ASSERT_TRUE(onlyOneMatches(sharedResourcesNodeCondition()));

    // When access to the camera is taken from the custom user.
    setupAccessToResourceForUser(customUser, camera, false);

    // Then there are no shared resources groups found in the resource tree.
    ASSERT_TRUE(noneMatches(sharedResourcesNodeCondition()));
}

TEST_F(ResourceTreeModelTest, viewerUserResourcesPlaceholders)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When user with live viewer permissions and unique name is added to the resource pool.
    addUser(kUniqueUserName, GlobalPermission::liveViewerPermissions);

    // Then exactly one node with unique user name found in the resource tree.
    const auto liveViewerUserIndex = uniqueMatchingIndex(kUniqueUserNameCondition);

    // And that node always have single "All Cameras & Resources" placeholder node as child.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            allCamerasAndResourcesNodeCondition(),
            directChildOf(liveViewerUserIndex))));

    // At once, it doesn't have "All Shared Layouts" placeholder node as child.
    ASSERT_TRUE(noneMatches(
        allOf(
            allSharedLayoutsNodeCondition(),
            directChildOf(liveViewerUserIndex))));
}

TEST_F(ResourceTreeModelTest, adminUserResourcesPlaceholders)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When another administrator user with unique name is added to the resource pool.
    addUser(kUniqueUserName, GlobalPermission::adminPermissions);

    // Then exactly one node with unique user name found in the resource tree.
    const auto anotherAdminUserIndex = uniqueMatchingIndex(kUniqueUserNameCondition);

    // And that node always have single "All Cameras & Resources" placeholder node as child.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            allCamerasAndResourcesNodeCondition(),
            directChildOf(anotherAdminUserIndex))));

    // And "All Shared Layouts" placeholder node as well.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            allSharedLayoutsNodeCondition(),
            directChildOf(anotherAdminUserIndex))));
}

TEST_F(ResourceTreeModelTest, customUserResourcesPlaceholders)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When custom user with unique name is added to the resource pool.
    addUser(kUniqueUserName, GlobalPermission::customUser);

    // Then exactly one node with unique user name found in the resource tree.
    const auto customUserIndex = uniqueMatchingIndex(kUniqueUserNameCondition);

    // And it doesn't have placeholders within children nodes at all.
    ASSERT_TRUE(noneMatches(
        allOf(
            anyOf(allSharedLayoutsNodeCondition(), allCamerasAndResourcesNodeCondition()),
            directChildOf(customUserIndex))));
}

TEST_F(ResourceTreeModelTest, viewerUserSharedAndOwnLayoutsAreNotInGroup)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When user with live viewer permissions and unique name is added to the resource pool.
    const auto liveViewerUser = addUser(kUniqueUserName, GlobalPermission::liveViewerPermissions);

    // When unique named layout owned by live viewer user is added to the resource pool.
    addLayout(kUniqueLayoutName, liveViewerUser->getId());

    // When unique named shared layout is added to the resource pool.
    const auto sharedLayout = addLayout(kUniqueSharedLayoutName);

    // And that shared layout is made accessible by live viewer user.
    setupAccessToResourceForUser(liveViewerUser, sharedLayout, true);

    // Then exactly one node with unique user name found in the resource tree.
    const auto liveViewerIndex = uniqueMatchingIndex(kUniqueUserNameCondition);

    //Then both layouts listed as direct children of live viewer user node.
    ASSERT_TRUE(onlyOneMatches(
        allOf(directChildOf(liveViewerIndex), kUniqueLayoutNameCondition)));

    ASSERT_TRUE(onlyOneMatches(
        allOf(directChildOf(liveViewerIndex), kUniqueSharedLayoutNameCondition)));
}

TEST_F(ResourceTreeModelTest, layoutsGroupAppearsOnlyIfAccessibleLayoutsExistsForCustomUser)
{
    // When custom user with unique name is added to the resource pool.
    const auto customUser = addUser(kUniqueUserName, GlobalPermission::customUser);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then there are no custom user's layout groups found in the resource tree.
    ASSERT_TRUE(noneMatches(mixedLayoutsNodeCondition()));

    // When unique named layout owned by the custom user is added to the resource pool.
    const auto customUserLayout = addLayout(kUniqueLayoutName, customUser->getId());

    // Then layouts group appears as child of custom user node.
    const auto layoutsGroupIndex = uniqueMatchingIndex(
        allOf(mixedLayoutsNodeCondition(), directChildOf(kUniqueUserNameCondition)));

    // When only custom user layout is removed from the resource pool.
    resourcePool()->removeResource(customUserLayout);

    // Then again no more custom user's layout groups found in the resource tree.
    ASSERT_TRUE(noneMatches(mixedLayoutsNodeCondition()));
}

TEST_F(ResourceTreeModelTest, viewerUserLayoutNodeType)
{
    // When live viewer user added to the resource pool.
    const auto liveViewerUser = addUser("live_viewer", GlobalPermission::liveViewerPermissions);

    // When unique named layout owned by the live viewer user is added to the resource pool.
    const auto liveViewerLayout = addLayout(kUniqueLayoutName, liveViewerUser->getId());

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then single layout node outside 'Layouts' group found in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(allOf(
        kUniqueLayoutNameCondition,
        non(directChildOf(layoutsNodeCondition()))));

    // And that node has 'resource' node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::resource)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, viewerUserSharedLayoutNodeType)
{
    // When live viewer user added to the resource pool.
    const auto liveViewerUser = addUser("live_viewer", GlobalPermission::liveViewerPermissions);

    // When unique named shared layout is added to the resource pool.
    const auto sharedLayout = addLayout(kUniqueLayoutName);

    // And that shared layout is accessible by the live viewer user.
    setupAccessToResourceForUser(liveViewerUser, sharedLayout, true);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then single layout node outside 'Layouts' group found in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(allOf(
        kUniqueLayoutNameCondition,
        non(directChildOf(layoutsNodeCondition()))));

    // And that node has 'sharedLayout' node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::sharedLayout)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, customUserLayoutNodeType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When custom user is added to the resource pool.
    const auto customUser = addUser("custom_user", GlobalPermission::customUser);

    // When unique named layout owned by the custom user is added to the resource pool.
    const auto customUserLayout = addLayout(kUniqueLayoutName, customUser->getId());

    // Then single layout node outside 'Layouts' found in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(allOf(
        kUniqueLayoutNameCondition,
        non(directChildOf(layoutsNodeCondition()))));

    // And that node has 'resource' node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::resource)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, customUserSharedLayoutNodeType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When custom user is added to the resource pool.
    const auto customUser = addUser("custom_user", GlobalPermission::customUser);

    // When unique named shared layout is added to the resource pool.
    const auto sharedLayout = addLayout(kUniqueLayoutName);

    // And that shared layout is accessible by the custom user.
    setupAccessToResourceForUser(customUser, sharedLayout, true);

    // Then single layout node outside 'Layouts' found in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(allOf(
        kUniqueLayoutNameCondition,
        non(directChildOf(layoutsNodeCondition()))));

    // And that node has 'sharedLayout' node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::sharedLayout)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, customUserSharedAndOwnLayoutsAreGrouped)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When custom user with unique name is added to the resource pool.
    const auto customUser = addUser(kUniqueUserName, GlobalPermission::customUser);

    // When unique named layout owned by the custom user is added to the resource pool.
    addLayout(kUniqueLayoutName, customUser->getId());

    // When unique named shared layout is added to the resource pool.
    const auto sharedLayout = addLayout(kUniqueSharedLayoutName);

    // And that shared layout is made accessible by the custom user.
    setupAccessToResourceForUser(customUser, sharedLayout, true);

    // Then exactly one node with unique user name found in the resource tree.
    const auto customUserIndex = uniqueMatchingIndex(kUniqueUserNameCondition);

    // Then exactly one mixed layouts group node found as child of custom user node.
    const auto layoutsGroupIndex = uniqueMatchingIndex(
        allOf(mixedLayoutsNodeCondition(), directChildOf(customUserIndex)));

    // Then both layouts listed as children of layouts group.
    ASSERT_TRUE(onlyOneMatches(
        allOf(directChildOf(layoutsGroupIndex), kUniqueLayoutNameCondition)));

    ASSERT_TRUE(onlyOneMatches(
        allOf(directChildOf(layoutsGroupIndex), kUniqueSharedLayoutNameCondition)));

    // And no layouts found as direct children of user node.
    ASSERT_TRUE(noneMatches(
        allOf(directChildOf(customUserIndex), iconTypeMatch(QnResourceIconCache::Layout))));
}

TEST_F(ResourceTreeModelTest, userResourcesRebuildsOnUserPermissionsChange)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When user with live viewer permissions and unique name is added to the resource pool.
    const auto user = addUser(kUniqueUserName, GlobalPermission::liveViewerPermissions);

    // When unique named shared layout is added to the resource pool.
    const auto sharedLayout = addLayout(kUniqueLayoutName);

    // And that shared layout is made accessible by the live viewer user.
    setupAccessToResourceForUser(user, sharedLayout, true);

    // When administrator permissions granted to the user with unique user name.
    user->setRawPermissions(GlobalPermission::adminPermissions);

    // Then exactly one node with unique user name found in the resource tree.
    const auto userIndex = uniqueMatchingIndex(kUniqueUserNameCondition);

    // And it hasn't live viewer layout as a child.
    ASSERT_TRUE(noneMatches(
        allOf(kUniqueLayoutNameCondition, directChildOf(userIndex))));

    // But have "All Shared Layouts" placeholder node as administrator users do.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            allSharedLayoutsNodeCondition(),
            directChildOf(userIndex))));
}

} // namespace test
} // namespace nx::vms::client::desktop
