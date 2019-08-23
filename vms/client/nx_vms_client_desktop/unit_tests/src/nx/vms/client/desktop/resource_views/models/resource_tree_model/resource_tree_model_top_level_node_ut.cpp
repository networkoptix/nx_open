#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>

#include <ui/style/resource_icon_cache.h>

#include <api/runtime_info_manager.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

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

TEST_F(ResourceTreeModelTest, singleServerShouldBeTopLevelNode)
{
    // String constants.
    static constexpr auto kServerName = "test_server";

    // Set up environment.
    loginAsAdmin("admin");
    addServer(kServerName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kServerName),
            iconTypeMatch(QnResourceIconCache::Server),
            topLevelNode(),
            atRow(3))));
}

TEST_F(ResourceTreeModelTest, shouldGroupServersIfNotSingle)
{
    // String constants.
    static constexpr auto kServerName1 = "test_server_1";
    static constexpr auto kServerName2 = "test_server_2";

    // Set up environment.
    loginAsAdmin("admin");
    addServer(kServerName1);
    addServer(kServerName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch("Servers"),
            iconFullMatch(QnResourceIconCache::Servers),
            topLevelNode(),
            hasChildren())));

    ASSERT_EQ(2, matchCount(
        allOf(
            anyOf(displayFullMatch(kServerName1), displayFullMatch(kServerName2)),
            iconTypeMatch(QnResourceIconCache::Server),
            noneOf(topLevelNode()))));
}

TEST_F(ResourceTreeModelTest, localFilesNodeVisibility)
{
    // Perform actions and checks.

    // TODO: #vbreus On a first sight there should be a zero value, but actual representation of
    // the resource tree depends not only model state, but also on the root node set to the view.
    // Need to figure out how to achieve one to one correspondence of testing environment model and
    // actually displayed hierarchy.
    ASSERT_TRUE(onlyOneMatches(localFilesNodeCondition()));
    loginAsAdmin("admin");

    ASSERT_TRUE(onlyOneMatches(localFilesNodeCondition()));
    logout();

    // Same as above.
    ASSERT_TRUE(onlyOneMatches(localFilesNodeCondition()));
}

TEST_F(ResourceTreeModelTest, webPagesNodeAreAlwaysDisplayedIfLoggedIn)
{
    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(webPagesNodeCondition()));
    ASSERT_TRUE(noneMatches(directChildOf(webPagesNodeCondition())));

    loginAsLiveViewer("live_viewer");
    ASSERT_TRUE(onlyOneMatches(webPagesNodeCondition()));
    ASSERT_TRUE(noneMatches(directChildOf(webPagesNodeCondition())));

    logout();
    ASSERT_TRUE(noneMatches(webPagesNodeCondition()));
}

TEST_F(ResourceTreeModelTest, layoutsNodeAreDisplayedOnlyIfLayotsExist)
{
    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(noneMatches(layoutsNodeCondition()));

    addLayout("layout");
    ASSERT_TRUE(onlyOneMatches(layoutsNodeCondition()));
    ASSERT_TRUE(onlyOneMatches(directChildOf(layoutsNodeCondition())));

    logout();
    ASSERT_TRUE(noneMatches(layoutsNodeCondition()));
}

TEST_F(ResourceTreeModelTest, showreelsNodeAreDisplayedOnlyIfShowreelsExist)
{
    // Perform actions and checks.
    const auto user = loginAsAdmin("admin");
    const auto userId = user->getId();
    ASSERT_TRUE(noneMatches(showreelsNodeCondition()));

    addLayoutTour("showreel", userId);
    ASSERT_TRUE(onlyOneMatches(showreelsNodeCondition()));
    ASSERT_TRUE(onlyOneMatches(directChildOf(showreelsNodeCondition())));

    logout();
    ASSERT_TRUE(noneMatches(showreelsNodeCondition()));
}

// TODO: #vbreus "Other Systems" is missing
TEST_F(ResourceTreeModelTest, topLevelNodesOrder)
{
    // String constants.
    static constexpr auto kSystemName = "test_system";
    static constexpr auto kUserName = "test_admin";
    static constexpr auto kServerName = "test_server";
    static constexpr auto kVideowallName = "videowall";

    // Set up environment.
    setSystemName(kSystemName);
    const auto user = loginAsAdmin(kUserName);
    const auto userId = user->getId();
    addServer(kServerName);
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
        "Local Files"};

    // Check tree.
    ASSERT_EQ(transformToDisplayStrings(allMatchingIndexes(topLevelNode())), referenceSequence);
}
