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
using namespace nx::vms::client::desktop::index_condition;

TEST_F(ResourceTreeModelTest, showreelAdds)
{
    // String constants.
    static constexpr auto showreelName = "unique_showreel_name";

    // Set up environment.
    loginAsAdmin("admin");
    addLayoutTour(showreelName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(showreelName)));
}

TEST_F(ResourceTreeModelTest, showreelRemoves)
{
    // String constants.
    static constexpr auto showreelName = "unique_showreel_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto showreel = addLayoutTour(showreelName);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(showreelName)));

    commonModule()->layoutTourManager()->removeTour(showreel.id);
    ASSERT_TRUE(noneMatches(displayFullMatch(showreelName)));
}

TEST_F(ResourceTreeModelTest, showreelIcon)
{
    // String constants.
    static constexpr auto showreelName = "unique_showreel_name";

    // Set up environment.
    loginAsAdmin("admin");
    addLayoutTour(showreelName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(showreelName),
            iconTypeMatch(QnResourceIconCache::LayoutTour))));
}

TEST_F(ResourceTreeModelTest, showreelsAreChildrenOfCorrespondingTopLevelNode)
{
    // String constants.
    static constexpr auto showreelName1 = "unique_showreel_name_1";
    static constexpr auto showreelName2 = "unique_showreel_name_2";

    // Set up environment.
    loginAsAdmin("admin");
    addLayoutTour(showreelName1);
    addLayoutTour(showreelName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(showreelsNodeCondition()),
            displayFullMatch(showreelName1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(showreelsNodeCondition()),
            displayFullMatch(showreelName2))));
}
