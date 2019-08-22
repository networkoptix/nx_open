#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

TEST_F(ResourceTreeModelTest, layoutModificationMark)
{
    // String constants.
    static constexpr auto kLayoutName = "unique_layout_name";
    static constexpr auto kModifiedLayoutName = "modified_layout_name";

    // Set up environment.
    const auto user = loginAsAdmin("admin");
    const auto userId = user->getId();
    addLayout(kLayoutName, userId);
    auto modifiedLayout = addLayout(kModifiedLayoutName, userId);
    layoutSnapshotManager()->markChanged(modifiedLayout->getId(), true);

    // Check tree.
    ASSERT_EQ(2, matchCount(
        allOf(
            directChildOf(layoutsNodeCondition()))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayStartsWith(kModifiedLayoutName),
            displayEndsWith("*"),
            directChildOf(layoutsNodeCondition()))));
}
