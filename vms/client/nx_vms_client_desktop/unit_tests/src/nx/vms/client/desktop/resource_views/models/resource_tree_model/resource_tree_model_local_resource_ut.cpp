#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <ui/style/resource_icon_cache.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

TEST_F(ResourceTreeModelTest, offlineMediaResourcesAreNotDisplayed)
{
    // String constants.
    static constexpr auto kImageFilePath = "picture.bmp";
    static constexpr auto kVideoFilePath = "video.mov";
    static constexpr auto kLayoutFilePath = "layout.nov";

    // Set up environment.
    loginAsAdmin("admin");
    addLocalMedia(kImageFilePath)->setStatus(Qn::Offline);
    addLocalMedia(kVideoFilePath)->setStatus(Qn::Offline);
    addLocalMedia(kLayoutFilePath)->setStatus(Qn::Offline);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch("Local Files"),
            iconFullMatch(QnResourceIconCache::LocalResources),
            topLevelNode(),
            noneOf(hasChildren()))));

    ASSERT_TRUE(noneMatches(
        anyOf(
            displayContains(kImageFilePath),
            displayContains(kVideoFilePath),
            displayContains(kLayoutFilePath))));
}

TEST_F(ResourceTreeModelTest, localResourceVisibilityTransitions)
{
    // String constants.
    static constexpr auto kResourceName = "test_resource.mkv";

    // Reference conditions.
    const auto condition = displayFullMatch(kResourceName);

    // Perform actions and checks.
    const auto mediaResource = addLocalMedia(kResourceName);
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
    addLocalMedia("1_picture.png")->setStatus(Qn::Online);
    addLocalMedia("z_picture.jpg")->setStatus(Qn::Online);
    addLocalMedia("2_video.avi")->setStatus(Qn::Online);
    addLocalMedia("z_video.mkv")->setStatus(Qn::Online);
    addFileLayout("3_layout.nov")->setStatus(Qn::Online);
    addFileLayout("z_encrypted_layout.nov",/*isEncrypted*/ true)->setStatus(Qn::Online);

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
    addLocalMedia("test_directory/picture.png")->setStatus(Qn::Online);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(localFilesNodeCondition()),
            displayFullMatch("picture.png"))));
}
