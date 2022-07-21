// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>

#include <nx/vms/client/desktop/style/resource_icon_cache.h>

#include <api/runtime_info_manager.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

TEST_F(ResourceTreeModelTest, shouldShowPinnedNodesIfLoggedIn)
{
    // String constants.
    static constexpr auto kSystemName = "test_system";
    static constexpr auto kUserName = "test_admin";

    // Set up environment.
    setSystemName(kSystemName);
    loginAsAdmin(kUserName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kSystemName),
            iconFullMatch(QnResourceIconCache::CurrentSystem),
            topLevelNode(),
            atRow(0))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kUserName),
            iconFullMatch(QnResourceIconCache::User),
            topLevelNode(),
            atRow(1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayEmpty(),
            iconFullMatch(QnResourceIconCache::Unknown),
            topLevelNode(),
            atRow(2))));
}

TEST_F(ResourceTreeModelTest, localFilesNodeVisibility)
{
    // When no user is logged in.

    // There is no "Local Files" group node found in the tree.
    ASSERT_TRUE(noneMatches(localFilesNodeCondition()));

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then "Local Files" group node appears in the tree.
    ASSERT_TRUE(onlyOneMatches(localFilesNodeCondition()));

    // When no user is logged in again.
    logout();

    // There is no "Local Files" group node found in the tree.
    ASSERT_TRUE(noneMatches(localFilesNodeCondition()));
}

TEST_F(ResourceTreeModelTest, webPagesNodeAreAlwaysDisplayedIfLoggedIn)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then there is single "Web Pages" node in the tree with no children.
    ASSERT_TRUE(onlyOneMatches(webPagesNodeCondition()));
    ASSERT_TRUE(noneMatches(directChildOf(webPagesNodeCondition())));

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then there is single "Web Pages" node in the tree with no children.
    ASSERT_TRUE(onlyOneMatches(webPagesNodeCondition()));
    ASSERT_TRUE(noneMatches(directChildOf(webPagesNodeCondition())));

    // When no one is logged in.
    logout();

    // Then there is no "Web Pages" node in the tree.
    ASSERT_TRUE(noneMatches(webPagesNodeCondition()));
}

TEST_F(ResourceTreeModelTest, layoutsNodeAreDisplayedOnlyIfLayotsExist)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // Then there is no "Layouts" node in the tree.
    ASSERT_TRUE(noneMatches(layoutsNodeCondition()));

    // When layout is added to the resource pool.
    addLayout("layout");

    // Then there is single "Layouts" node in the tree.
    const auto layoutsIndex = uniqueMatchingIndex(layoutsNodeCondition());

    // And that node has children.
    ASSERT_TRUE(hasChildren()(layoutsIndex));
}

TEST_F(ResourceTreeModelTest, showreelsNodeIsDisplayedOnlyIfShowreelsExist)
{
    // When user is logged in.
    const auto user = loginAsAdmin("admin");

    // Then there is no "Showreels" node in the tree.
    ASSERT_TRUE(noneMatches(showreelsNodeCondition()));

    // When showreel is added.
    addLayoutTour("showreel", user->getId());

    // Then there is single "Showreels" node in the tree.
    const auto showreelsIndex = uniqueMatchingIndex(showreelsNodeCondition());

    // And that node has children.
    ASSERT_TRUE(hasChildren()(showreelsIndex));
}

// TODO: #vbreus "Other Systems" is missing
TEST_F(ResourceTreeModelTest, topLevelNodesOrder)
{
    // String constants.
    static constexpr auto kSystemName = "test_system";
    static constexpr auto kUserName = "test_owner";
    static constexpr auto kServerName = "test_server";
    static constexpr auto kFakeServerName = "fake_server";
    static constexpr auto kVideowallName = "videowall";

    // Set up environment.
    setSystemName(kSystemName);
    const auto user = loginAsOwner(kUserName);
    const auto userId = user->getId();
    addServer(kServerName);
    addFakeServer(kFakeServerName);
    addLayout("layout", userId);
    addVideoWall(kVideowallName);
    addLayoutTour("showreel", userId);

    // Reference data.
    std::vector<QString> referenceSequence = {
        kSystemName,
        kUserName,
        "",
        kServerName,
        "Layouts",
        "Showreels",
        kVideowallName,
        "Web Pages",
        "Users",
        "Other Systems",
        "Local Files"};

    // Check tree.
    ASSERT_EQ(transformToDisplayStrings(allMatchingIndexes(topLevelNode())), referenceSequence);
}

} // namespace test
} // namespace nx::vms::client::desktop
