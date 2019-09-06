#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>

#include <ui/style/resource_icon_cache.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::index_condition;

TEST_F(ResourceTreeModelTest, shouldShowPinnedNodesIfLoggedIn)
{
    // String constants.
    static constexpr auto systemName = "test_system";
    static constexpr auto userName = "test_admin";

    // Set up environment.
    setSystemName(systemName);
    loginAsAdmin(userName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(systemName),
            iconFullMatch(QnResourceIconCache::CurrentSystem),
            topLevelNode(),
            atRow(0))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(userName),
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
    static constexpr auto serverName = "test_server";

    // Set up environment.
    loginAsAdmin("admin");
    addServer(serverName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(serverName),
            iconTypeMatch(QnResourceIconCache::Server),
            topLevelNode(),
            atRow(3))));
}

TEST_F(ResourceTreeModelTest, shouldGroupServersIfNotSingle)
{
    // String constants.
    static constexpr auto serverName1 = "test_server_1";
    static constexpr auto serverName2 = "test_server_2";

    // Set up environment.
    loginAsAdmin("admin");
    addServer(serverName1);
    addServer(serverName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch("Servers"),
            iconFullMatch(QnResourceIconCache::Servers),
            topLevelNode(),
            hasChildren())));

    ASSERT_EQ(2, matchCount(
        allOf(
            anyOf(displayFullMatch(serverName1), displayFullMatch(serverName2)),
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

TEST_F(ResourceTreeModelTest, offlineMediaResourcesAreNotDisplayed)
{
    // String constants.
    static constexpr auto imageFilePath = "picture.bmp";
    static constexpr auto videoFilePath = "video.mov";
    static constexpr auto layoutFilePath = "layout.nov";

    // Set up environment.
    loginAsAdmin("admin");
    addMediaResource(imageFilePath)->setStatus(Qn::Offline);
    addMediaResource(videoFilePath)->setStatus(Qn::Offline);
    addMediaResource(layoutFilePath)->setStatus(Qn::Offline);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch("Local Files"),
            iconFullMatch(QnResourceIconCache::LocalResources),
            topLevelNode(),
            noneOf(hasChildren()))));

    ASSERT_TRUE(noneMatches(
        anyOf(
            displayContains(imageFilePath),
            displayContains(videoFilePath),
            displayContains(layoutFilePath))));
}

TEST_F(ResourceTreeModelTest, localResourceVisibilityTransitions)
{
    // String constants.
    static constexpr auto resourceName = "test_resource.mkv";

    // Reference conditions.
    const auto condition = displayFullMatch(resourceName);

    // Perform actions and checks.
    const auto mediaResource = addMediaResource(resourceName);
    mediaResource->setStatus(Qn::Online);
    ASSERT_TRUE(onlyOneMatches(condition));

    mediaResource->setStatus(Qn::Offline);
    ASSERT_TRUE(noneMatches(condition));

    mediaResource->setStatus(Qn::Online);
    ASSERT_TRUE(onlyOneMatches(condition));
}

TEST_F(ResourceTreeModelTest, localResourcesIconsAndGrouping)
{
    // Set up environment.
    loginAsAdmin("admin");
    addMediaResource("1_picture.png")->setStatus(Qn::Online);
    addMediaResource("z_picture.jpg")->setStatus(Qn::Online);
    addMediaResource("2_video.avi")->setStatus(Qn::Online);
    addMediaResource("z_video.mkv")->setStatus(Qn::Online);
    addFileLayoutResource("3_layout.nov")->setStatus(Qn::Online);
    addFileLayoutResource("z_encrypted_layout.nov",/*isEncrypted*/ true)->setStatus(Qn::Online);

    // Reference conditions.
    const auto fileLayoutCondition =
        allOf(
            directChildOf(localFilesNodeCondition()),
            anyOf(
                iconTypeMatch(QnResourceIconCache::ExportedLayout),
                iconTypeMatch(QnResourceIconCache::ExportedEncryptedLayout)));

    const auto localVideoCondition =
        allOf(
            directChildOf(localFilesNodeCondition()),
            iconTypeMatch(QnResourceIconCache::Media));

    const auto localImageCondition =
        allOf(
            directChildOf(localFilesNodeCondition()),
            iconTypeMatch(QnResourceIconCache::Image));

    // Check tree.
    ASSERT_GT(
        allMatchingIndexes(localVideoCondition).first().row(),
        allMatchingIndexes(fileLayoutCondition).last().row());

    ASSERT_GT(
        allMatchingIndexes(localImageCondition).first().row(),
        allMatchingIndexes(localVideoCondition).last().row());

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            fileLayoutCondition,
            iconTypeMatch(QnResourceIconCache::ExportedEncryptedLayout))));
}

TEST_F(ResourceTreeModelTest, mediaResourceOnlyFilenameDisplayed)
{
    // Set up environment.
    loginAsAdmin("admin");
    addMediaResource("test_directory/picture.png")->setStatus(Qn::Online);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(localFilesNodeCondition()),
            displayFullMatch("picture.png"))));
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

    addLayoutResource("layout");
    ASSERT_TRUE(onlyOneMatches(layoutsNodeCondition()));
    ASSERT_TRUE(onlyOneMatches(directChildOf(layoutsNodeCondition())));

    logout();
    ASSERT_TRUE(noneMatches(layoutsNodeCondition()));
}

TEST_F(ResourceTreeModelTest, videoWallAdds)
{
    // Perform actions and checks.
    loginAsAdmin("admin");
    addVideoWallResource("videowall1");
    ASSERT_TRUE(onlyOneMatches(videoWallNodeCondition()));

    addVideoWallResource("videowall1");
    ASSERT_EQ(2, matchCount(videoWallNodeCondition()));
}

TEST_F(ResourceTreeModelTest, layoutModificationMark)
{
    // String constants.
    static constexpr auto modifiedLayoutName = "modified_layout";

    // Set up environment.
    const auto user = loginAsAdmin("admin");
    const auto userId = user->getId();
    addLayoutResource("layout", userId);
    auto modifiedLayout = addLayoutResource(modifiedLayoutName, userId);
    modifiedLayout->setCellSpacing(0.0);

    // Check tree.
    ASSERT_EQ(2, matchCount(
        allOf(
            directChildOf(layoutsNodeCondition()))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayContains(modifiedLayoutName),
            displayEndsWith("*"),
            directChildOf(layoutsNodeCondition()))));
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
    static constexpr auto systemName = "test_system";
    static constexpr auto userName = "test_admin";
    static constexpr auto serverName = "test_server";
    static constexpr auto videowallName = "videowall";

    // Set up environment.
    setSystemName(systemName);
    const auto user = loginAsAdmin(userName);
    const auto userId = user->getId();
    addServer(serverName);
    addLayoutResource("layout", userId);
    addVideoWallResource(videowallName);
    addLayoutTour("showreel", userId);

    // Reference data.
    std::vector<QString> referenceSequence = {
        systemName,
        userName,
        "",
        serverName,
        "Layouts",
        "Showreels",
        videowallName,
        "Web Pages",
        "Users",
        "Local Files"};

    // Check tree.
    ASSERT_EQ(transformToDisplayStrings(allMatchingIndexes(topLevelNode())), referenceSequence);
}
