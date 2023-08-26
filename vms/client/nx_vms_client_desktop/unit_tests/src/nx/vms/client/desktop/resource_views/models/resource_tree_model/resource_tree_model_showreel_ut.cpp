// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/showreel/showreel_manager.h>

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueShowreelName = "unique_showreel_name";

// Predefined conditions.
static const auto kUniqueShowreelNameCondition = displayFullMatch(kUniqueShowreelName);

TEST_F(ResourceTreeModelTest, showreelAdds)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueShowreelName)));
}

TEST_F(ResourceTreeModelTest, showreelRemoves)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    const auto showreel = addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueShowreelName)));

    // When previously added showreel is removed.
    systemContext()->showreelManager()->removeShowreel(showreel.id);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueShowreelName)));
}

TEST_F(ResourceTreeModelTest, showreelIconType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    const auto showreel = addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto showreelIndex = uniqueMatchingIndex(kUniqueShowreelNameCondition);

    // And it have showreel icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::Showreel)(showreelIndex));
}

TEST_F(ResourceTreeModelTest, showreelIsChildrenOfCorrespondingTopLevelNode)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto showreelIndex = uniqueMatchingIndex(kUniqueShowreelNameCondition);

    // And this node is child of "Showreels" node.
    ASSERT_TRUE(directChildOf(showreelsNodeCondition())(showreelIndex));
}

TEST_F(ResourceTreeModelTest, showreelIsNotDisplayedIfNotLoggedIn)
{
    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueShowreelName)));

    // When no user is logged in.
    logout();

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueShowreelName)));

    // Then no more nodes with showreel icon type found in the resource tree.
    ASSERT_TRUE(noneMatches(iconTypeMatch(QnResourceIconCache::Showreel)));
}

TEST_F(ResourceTreeModelTest, showreelTooltip)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto showreelIndex = uniqueMatchingIndex(kUniqueShowreelNameCondition);

    // And that node have same tooltip text as display text.
    ASSERT_TRUE(dataMatch(Qt::ToolTipRole, kUniqueShowreelName)(showreelIndex));
}

TEST_F(ResourceTreeModelTest, showreelDisplayNameMapping)
{
    // When showreel with certain unique name is added.
    auto showreel = addShowreel(kUniqueShowreelName);

    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueShowreelName)));

    // And that node have same display text as Showreel::getName() returns.
    ASSERT_EQ(showreel.name, kUniqueShowreelName);
}

TEST_F(ResourceTreeModelTest, showreelItemIsDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto showreelIndex = uniqueMatchingIndex(kUniqueShowreelNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(showreelIndex));
}

TEST_F(ResourceTreeModelTest, showreelHasItemNeverHasChildrenFlag)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto showreelIndex = uniqueMatchingIndex(kUniqueShowreelNameCondition);

    // And that node is declared as leaf node.
    ASSERT_TRUE(hasFlag(Qt::ItemNeverHasChildren)(showreelIndex));
}

TEST_F(ResourceTreeModelTest, showreelItemIsNotDropEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When showreel with certain unique name is added.
    addShowreel(kUniqueShowreelName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto showreelIndex = uniqueMatchingIndex(kUniqueShowreelNameCondition);

    // And that node isn't accept drop action.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDropEnabled)(showreelIndex));
}

} // namespace test
} // namespace nx::vms::client::desktop
