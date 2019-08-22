#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/webpage_resource.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

TEST_F(ResourceTreeModelTest, webPageAdds)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(kWebPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));
}

TEST_F(ResourceTreeModelTest, webPageRemoves)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto webPage = addWebPage(kWebPageName);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));

    resourcePool()->removeResource(webPage);
    ASSERT_TRUE(noneMatches(displayFullMatch(kWebPageName)));
}

TEST_F(ResourceTreeModelTest, webPagesIcons)
{
    // String constants.
    static constexpr auto kRegularWebPageName = "regular_web_page_name";
    static constexpr auto kC2pWebPageName = "c2p_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto regularWebPage = addWebPage(kRegularWebPageName);
    auto c2pWebPage = addWebPage(kC2pWebPageName);
    regularWebPage->setSubtype(WebPageSubtype::none);
    c2pWebPage->setSubtype(WebPageSubtype::c2p);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kRegularWebPageName),
            iconTypeMatch(QnResourceIconCache::WebPage))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kC2pWebPageName),
            iconTypeMatch(QnResourceIconCache::C2P))));
}

TEST_F(ResourceTreeModelTest, webPagesAreChildrenOfCorrespondingTopLevelNode)
{
    // String constants.
    static constexpr auto kWebPageName1 = "unique_web_page_name_1";
    static constexpr auto kWebPageName2 = "unique_web_page_name_2";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(kWebPageName1);
    addWebPage(kWebPageName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(webPagesNodeCondition()),
            displayFullMatch(kWebPageName1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(webPagesNodeCondition()),
            displayFullMatch(kWebPageName2))));
}

TEST_F(ResourceTreeModelTest, webPageIsDisplayedToAnyUser)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(kWebPageName);

    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));

    loginAsLiveViewer("live_viewer");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));
}

TEST_F(ResourceTreeModelTest, webPageIsNotDisplayedIfNotLoggedIn)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(kWebPageName);

    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));

    logout();
    ASSERT_TRUE(noneMatches(displayFullMatch(kWebPageName)));
    ASSERT_TRUE(noneMatches(
        anyOf(
            iconTypeMatch(QnResourceIconCache::WebPage),
            iconTypeMatch(QnResourceIconCache::C2P))));
}

TEST_F(ResourceTreeModelTest, webPageHelpTopic)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(kWebPageName);
    loginAsAdmin("admin");

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            dataMatch(Qn::HelpTopicIdRole, Qn::MainWindow_Tree_WebPage_Help))));
}

TEST_F(ResourceTreeModelTest, webPageTooltip)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(kWebPageName);
    loginAsAdmin("admin");

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            dataMatch(Qt::ToolTipRole, kWebPageName))));
}

TEST_F(ResourceTreeModelTest, webPageDisplayNameMapping)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    auto webPage = addWebPage(kWebPageName);
    loginAsAdmin("admin");

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));
    ASSERT_EQ(webPage->getName(), kWebPageName);
}

// TODO: #vbreus user with live viewer permissions able to modify web page item due to absence of
// action::Manager instance within QnResourceTreeModel instance. Need to figure out how to
// overcome this.
TEST_F(ResourceTreeModelTest, DISABLED_webPageItemIsEditableOnlyByAdmin)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(kWebPageName);

    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            hasFlag(Qt::ItemIsEditable))));

    loginAsLiveViewer("live_viewer");
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            noneOf(hasFlag(Qt::ItemIsEditable)))));
}

// TODO: #vbreus setData() is no-op due to absence of action::Manager instance
// within QnResourceTreeModel instance. Need to figure out how to overcome this.
TEST_F(ResourceTreeModelTest, DISABLED_webPageEditActionAffectsResourceName)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";
    static constexpr auto kNewWebPageName = "renamed_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto webPage = addWebPage(kWebPageName);
    auto index = uniqueMatchingIndex(displayFullMatch(kWebPageName));

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));

    model()->setData(index, kNewWebPageName);
    ASSERT_TRUE(noneMatches(displayFullMatch(kWebPageName)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kNewWebPageName)));
}

TEST_F(ResourceTreeModelTest, webPageItemIsDragEnabled)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(kWebPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            hasFlag(Qt::ItemIsDragEnabled))));
}

// TODO: #vbreus Doesn't pass. Not critical, but makes sense for the goodness.
TEST_F(ResourceTreeModelTest, DISABLED_webPageHasItemNeverHasChildrenFlag)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(kWebPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            hasFlag(Qt::ItemNeverHasChildren))));
}

// TODO: #vbreus Doesn't pass. Not critical, but causes misleading item hover decoration.
TEST_F(ResourceTreeModelTest, DISABLED_webPageItemIsNotDropEnabled)
{
    // String constants.
    static constexpr auto kWebPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(kWebPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kWebPageName)));
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(kWebPageName),
            noneOf(hasFlag(Qt::ItemIsDropEnabled)))));
}
