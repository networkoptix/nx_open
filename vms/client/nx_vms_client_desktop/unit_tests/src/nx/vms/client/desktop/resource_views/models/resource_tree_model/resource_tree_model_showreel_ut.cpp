#include "resource_tree_model_test_fixture.h"

#include <common/common_module.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/layout_tour_data.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

TEST_F(ResourceTreeModelTest, showreelAdds)
{
    // String constants.
    static constexpr auto kShowreelName = "unique_showreel_name";

    // Set up environment.
    loginAsAdmin("admin");
    addLayoutTour(kShowreelName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kShowreelName)));
}

TEST_F(ResourceTreeModelTest, showreelRemoves)
{
    // String constants.
    static constexpr auto kShowreelName = "unique_showreel_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto showreel = addLayoutTour(kShowreelName);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kShowreelName)));

    commonModule()->layoutTourManager()->removeTour(showreel.id);
    ASSERT_TRUE(noneMatches(displayFullMatch(kShowreelName)));
}

TEST_F(ResourceTreeModelTest, showreelIcon)
{
    // String constants.
    static constexpr auto kShowreelName = "unique_showreel_name";

    // Set up environment.
    loginAsAdmin("admin");
    addLayoutTour(kShowreelName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kShowreelName),
            iconTypeMatch(QnResourceIconCache::LayoutTour))));
}

TEST_F(ResourceTreeModelTest, showreelsAreChildrenOfCorrespondingTopLevelNode)
{
    // String constants.
    static constexpr auto kShowreelName1 = "unique_showreel_name_1";
    static constexpr auto kShowreelName2 = "unique_showreel_name_2";

    // Set up environment.
    loginAsAdmin("admin");
    addLayoutTour(kShowreelName1);
    addLayoutTour(kShowreelName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(showreelsNodeCondition()),
            displayFullMatch(kShowreelName1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(showreelsNodeCondition()),
            displayFullMatch(kShowreelName2))));
}
