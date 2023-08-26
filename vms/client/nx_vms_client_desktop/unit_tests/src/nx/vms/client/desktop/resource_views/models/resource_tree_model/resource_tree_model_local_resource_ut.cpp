// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <client/client_globals.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueImageUrl = "unique_image_url.png";
static constexpr auto kUniqueVideoUrl = "unique_video_url.avi";
static constexpr auto kUniqueFileLayoutUrl = "unique_file_layout_url.nov";
static constexpr auto kUniqueEncryptedFileLayoutUrl = "unique_encrypted_file_layout_url.nov";

TEST_F(ResourceTreeModelTest, mediaResourceAdds)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(imageResource->getName())));

    // When QnAviResource with certain unique video url is added to the resource pool.
    const auto videoResource = addLocalMedia(kUniqueVideoUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(videoResource->getName())));
}

TEST_F(ResourceTreeModelTest, fileLayoutResourceAdds)
{
    // When QnFileLayoutResource with certain unique file layout url is added to the resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(fileLayoutResource->getName())));

    // When QnFileLayoutResource with certain unique file layout url and encrypted flag set on
    // is added to the resource pool.
    const auto encryptedFileLayoutResource = addFileLayout(kUniqueFileLayoutUrl, true);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(encryptedFileLayoutResource->getName())));
}

TEST_F(ResourceTreeModelTest, mediaResourceRemoves)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto mediaResource = addLocalMedia(kUniqueVideoUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(mediaResource->getName())));

    // When previously added QnAviResource is removed from resource pool.
    resourcePool()->removeResource(mediaResource);

    // Then no more nodes with corresponding display text found in the Resource Tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(mediaResource->getName())));
}

TEST_F(ResourceTreeModelTest, fileLayoutResourceRemoves)
{
    // When file layout with certain unique url is added to the resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(fileLayoutResource->getName())));

    // When previously added file layout is removed from resource pool.
    resourcePool()->removeResource(fileLayoutResource);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(fileLayoutResource->getName())));
}

TEST_F(ResourceTreeModelTest, fileLayoutItemAdds)
{
    // When file layout with certain unique url is added to the resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);

    // When video resource with certain unique url is added to the resource pool.
    const auto videoResource = addLocalMedia(kUniqueVideoUrl);

    // When that video resource is added to previously added file layout
    addToLayout(fileLayoutResource, {videoResource});

    // Than that video resource is represented as child of file layout
    const auto fileLayoutIndex = uniqueMatchingIndex(displayContains(fileLayoutResource->getName()));
    const auto childVideoIndex = uniqueMatchingIndex(directChildOf(fileLayoutIndex));
}

TEST_F(ResourceTreeModelTest, fileLayoutItemRemoves)
{
    // When file layout with certain unique url is added to the resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);

    // When video resource with certain unique url is added to the resource pool.
    const auto videoResource = addLocalMedia(kUniqueVideoUrl);

    // When that video resource is added to previously added file layout
    addToLayout(fileLayoutResource, {videoResource});

    // Than that video resource is represented as child of file layout
    auto fileLayoutIndex =
        QPersistentModelIndex(uniqueMatchingIndex(displayContains(fileLayoutResource->getName())));
    const auto childVideoIndex = uniqueMatchingIndex(directChildOf(fileLayoutIndex));

    // When video resource is removed from the resource pool.
    resourcePool()->removeResource(videoResource);

    // Than only childless file layout remains in the resource tree
    ASSERT_FALSE(hasChildren()(fileLayoutIndex));
}

TEST_F(ResourceTreeModelTest, mediaResourcesIcons)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // When QnAviResource with certain unique video url is added to the resource pool.
    const auto videoResource = addLocalMedia(kUniqueVideoUrl);

    // Then there is exactly one node with corresponding display text and image icon type appears
    // in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(imageResource->getName()),
            iconTypeMatch(QnResourceIconCache::Image))));

    // Then there is exactly one node with corresponding display text and video icon type appears
    // in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(videoResource->getName()),
            iconTypeMatch(QnResourceIconCache::Media))));
}

TEST_F(ResourceTreeModelTest, fileLayoutResourcesIcons)
{
    // When QnFileLayoutResource with certain unique file layout url is added to the resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);

    // When QnFileLayoutResource with certain unique file layout url and encrypted flag set on
    // is added to the resource pool.
    const auto encryptedFileLayoutResource = addFileLayout(kUniqueEncryptedFileLayoutUrl, true);

    // Then there is exactly one node with corresponding display text and file layout icon type
    // appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(fileLayoutResource->getName()),
            iconTypeMatch(QnResourceIconCache::ExportedLayout))));

    // Then there is exactly one node with corresponding display text and encrypted file layout
    // icon type appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(encryptedFileLayoutResource->getName()),
            iconTypeMatch(QnResourceIconCache::ExportedEncryptedLayout))));
}

TEST_F(ResourceTreeModelTest, mediaResourcesAreChildrenOfCorrespondingTopLevelNodeWhenLoggedIn)
{
    // When user is logged in.
    loginAsLiveViewer("live_viewer");

    // When both kinds of local media resources with different unique urls are added to the
    // resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);
    const auto videoResource = addLocalMedia(kUniqueVideoUrl);

    // Then "Local Files" top-level node have exactly two children with corresponding
    // display texts.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(localFilesNodeCondition()),
            displayFullMatch(imageResource->getName()))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(localFilesNodeCondition()),
            displayFullMatch(videoResource->getName()))));
}

TEST_F(ResourceTreeModelTest, fileLayoutsAreChildrenOfCorrespondingTopLevelNodeWhenLoggedIn)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When both kinds of file layout resources with different unique urls are added to the
    // resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);
    const auto encryptedFileLayoutResource = addFileLayout(kUniqueEncryptedFileLayoutUrl, true);

    // Then "Local Files" top-level node have exactly two children with corresponding
    // display texts.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(localFilesNodeCondition()),
            displayFullMatch(fileLayoutResource->getName()))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(localFilesNodeCondition()),
            displayFullMatch(encryptedFileLayoutResource->getName()))));
}

TEST_F(ResourceTreeModelTest, mediaResourceIsDisplayedToAnyUser)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(imageResource->getName())));

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(imageResource->getName())));
}

TEST_F(ResourceTreeModelTest, mediaResourceIsDisplayedIfNotLoggedIn)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(imageResource->getName())));

    // When no user is logged in.
    logout();

    // Then again exactly one node with corresponding display text appears in the Resource Tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(imageResource->getName())));
}

TEST_F(ResourceTreeModelTest, imageMediaResourceHelpTopic)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    // And that node provides certain help topic data.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(imageResource->getName()),
            dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::MainWindow_Tree_Exported))));
}

TEST_F(ResourceTreeModelTest, videoMediaResourceHelpTopic)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto videoResource = addLocalMedia(kUniqueVideoUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    // And that node provides certain help topic data.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(videoResource->getName()),
            dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::MainWindow_Tree_Exported))));
}

TEST_F(ResourceTreeModelTest, fileLayoutResourceHelpTopic)
{
    // When QnFileLayoutResource with certain unique file layout url is added to the resource pool.
    const auto fileLayoutResource = addFileLayout(kUniqueFileLayoutUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    // And that node provides certain help topic data.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(fileLayoutResource->getName()),
            dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::MainWindow_Tree_MultiVideo))));
}

TEST_F(ResourceTreeModelTest, mediaResourceTooltip)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    // And that node have same tooltip text as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(imageResource->getName()),
            dataMatch(Qt::ToolTipRole, imageResource->getName()))));
}

TEST_F(ResourceTreeModelTest, mediaResourceItemIsAlwaysEditable)
{
    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    // And that node is can be edited.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(imageResource->getName()),
            hasFlag(Qt::ItemIsEditable))));

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then exactly one node with corresponding display text appears in the Resource Tree.
    // And that node is can be edited.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(imageResource->getName()),
            hasFlag(Qt::ItemIsEditable))));
}

TEST_F(ResourceTreeModelTest, mediaResourceItemIsDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto imageIndex = uniqueMatchingIndex(displayFullMatch(imageResource->getName()));

    // And that node is declared as leaf node.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(imageIndex));
}

TEST_F(ResourceTreeModelTest, mediaResourceHasItemNeverHasChildrenFlag)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto imageIndex = uniqueMatchingIndex(displayFullMatch(imageResource->getName()));

    // And that node is declared as leaf node.
    ASSERT_TRUE(hasFlag(Qt::ItemNeverHasChildren)(imageIndex));
}

TEST_F(ResourceTreeModelTest, mediaResourceItemIsNotDropEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When QnAviResource with certain unique image url is added to the resource pool.
    const auto imageResource = addLocalMedia(kUniqueImageUrl);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto imageIndex = uniqueMatchingIndex(displayFullMatch(imageResource->getName()));

    // And that node isn't accept drop action.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDropEnabled)(imageIndex));
}

TEST_F(ResourceTreeModelTest, offlineMediaResourcesAreNotDisplayed)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When different kind local resources with offline status are added to the resource pool.
    addLocalMedia(kUniqueImageUrl)->setStatus(nx::vms::api::ResourceStatus::offline);
    addLocalMedia(kUniqueVideoUrl)->setStatus(nx::vms::api::ResourceStatus::offline);
    addFileLayout(kUniqueFileLayoutUrl)->setStatus(nx::vms::api::ResourceStatus::offline);

    // Then "Local Files" node have no children.
    auto localFilesNode = uniqueMatchingIndex(localFilesNodeCondition());
    ASSERT_FALSE(hasChildren()(localFilesNode));

    // Then local resources nodes not located anywhere else in the tree.
    ASSERT_TRUE(noneMatches(
        anyOf(
            displayContains(kUniqueImageUrl),
            displayContains(kUniqueVideoUrl),
            displayContains(kUniqueFileLayoutUrl))));
}

TEST_F(ResourceTreeModelTest, localResourceVisibilityTransitions)
{
    // When media resource with online status is added to the resource pool.
    const auto mediaResource = addLocalMedia(kUniqueVideoUrl);
    mediaResource->setStatus(nx::vms::api::ResourceStatus::online);

    // Then corresponding node appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(mediaResource->getName())));

    // When offline status set to the media resource.
    mediaResource->setStatus(nx::vms::api::ResourceStatus::offline);

    // Then media resource node disappears from the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(mediaResource->getName())));

    // When online status set to the media resource.
    mediaResource->setStatus(nx::vms::api::ResourceStatus::online);

    // Then media resource node appears in the resource tree again.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(mediaResource->getName())));
}

} // namespace test
} // namespace nx::vms::client::desktop
