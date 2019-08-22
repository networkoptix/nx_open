#include "resource_tree_model_sorting_test_fixture.h"

#include <QtCore/QCollator>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

TEST_P(ResourceTreeModelSortingTest, sorting)
{
    // String constants.
    std::vector<QString> sortingSignificantStrings = { "10_1", "2_11", "1_2", "b", "A", "C" };

    // Set up environment.
    const auto user = loginAsAdmin("admin");
    const auto userId = user->getId();
    for (const QString& string: sortingSignificantStrings)
    {
        addLayout(string, userId);
        addFileLayout(string + ".nov");
        addLocalMedia(string + ".png");
        addLocalMedia(string + ".mkv");
        addLayoutTour(string, userId);
        addVideoWall(string);
        addWebPage(string);
    }

    // Check tree.
    auto indexes = allMatchingIndexes(std::get<0>(GetParam()));
    ASSERT_TRUE(indexes.size() >= sortingSignificantStrings.size());

    std::vector<QString> displayedSequence = transformToDisplayStrings(indexes);

    std::vector<QString> properlySortedSequence = displayedSequence;
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(properlySortedSequence.begin(), properlySortedSequence.end(),
        [collator](const QString& l, const QString& r) { return collator.compare(l, r) < 0; });

    ASSERT_EQ(displayedSequence, properlySortedSequence);
}

// TODO: #vbreus Little messy.
INSTANTIATE_TEST_CASE_P(,
    ResourceTreeModelSortingTest, testing::Values(
        std::make_tuple(directChildOf(layoutsNodeCondition()), "Layouts"),
        std::make_tuple(videoWallNodeCondition(), "VideoWalls"),
        std::make_tuple(directChildOf(webPagesNodeCondition()), "WebPages"),
        std::make_tuple(directChildOf(showreelsNodeCondition()), "Showreels"),
        std::make_tuple(allOf(
            directChildOf(localFilesNodeCondition()),
            iconTypeMatch(QnResourceIconCache::Image)), "LocalImages"),
        std::make_tuple(allOf(
            directChildOf(localFilesNodeCondition()),
            iconTypeMatch(QnResourceIconCache::Media)), "LocalVideos"),
        std::make_tuple(allOf(
            directChildOf(localFilesNodeCondition()),
            anyOf(
                iconTypeMatch(QnResourceIconCache::ExportedLayout),
                iconTypeMatch(QnResourceIconCache::ExportedEncryptedLayout))), "FileLayouts")),
    ResourceTreeModelSortingTest::PrintToStringParamName());
