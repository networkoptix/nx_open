#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/webpage_resource.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::index_condition;

TEST_F(ResourceTreeModelTest, webPageAdds)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(webPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));
}

TEST_F(ResourceTreeModelTest, webPageRemoves)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto webPage = addWebPage(webPageName);

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));

    resourcePool()->removeResource(webPage);
    ASSERT_TRUE(noneMatches(displayFullMatch(webPageName)));
}

TEST_F(ResourceTreeModelTest, webPagesIcons)
{
    // String constants.
    static constexpr auto regularWebPageName = "regular_web_page_name";
    static constexpr auto c2pWebPageName = "c2p_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto regularWebPage = addWebPage(regularWebPageName);
    auto c2pWebPage = addWebPage(c2pWebPageName);
    regularWebPage->setSubtype(WebPageSubtype::none);
    c2pWebPage->setSubtype(WebPageSubtype::c2p);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(regularWebPageName),
            iconTypeMatch(QnResourceIconCache::WebPage))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(c2pWebPageName),
            iconTypeMatch(QnResourceIconCache::C2P))));
}

TEST_F(ResourceTreeModelTest, webPagesAreChildrenOfCorrespondingTopLevelNode)
{
    // String constants.
    static constexpr auto webPageName1 = "unique_web_page_name_1";
    static constexpr auto webPageName2 = "unique_web_page_name_2";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(webPageName1);
    addWebPage(webPageName2);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(webPagesNodeCondition()),
            displayFullMatch(webPageName1))));

    ASSERT_TRUE(onlyOneMatches(
        allOf(
            directChildOf(webPagesNodeCondition()),
            displayFullMatch(webPageName2))));
}

TEST_F(ResourceTreeModelTest, webPageIsDisplayedToAnyUser)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(webPageName);

    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));

    loginAsLiveViewer("live_viewer");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));
}

TEST_F(ResourceTreeModelTest, webPageIsNotDisplayedIfNotLoggedIn)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(webPageName);

    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));

    logout();
    ASSERT_TRUE(noneMatches(displayFullMatch(webPageName)));
    ASSERT_TRUE(noneMatches(
        anyOf(
            iconTypeMatch(QnResourceIconCache::WebPage),
            iconTypeMatch(QnResourceIconCache::C2P))));
}

TEST_F(ResourceTreeModelTest, webPageHelpTopic)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(webPageName);
    loginAsAdmin("admin");

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            dataMatch(Qn::HelpTopicIdRole, Qn::MainWindow_Tree_WebPage_Help))));
}

TEST_F(ResourceTreeModelTest, webPageTooltip)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(webPageName);
    loginAsAdmin("admin");

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            dataMatch(Qt::ToolTipRole, webPageName))));
}

TEST_F(ResourceTreeModelTest, webPageDisplayNameMapping)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    auto webPage = addWebPage(webPageName);
    loginAsAdmin("admin");

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));
    ASSERT_EQ(webPage->getName(), webPageName);
}

// TODO: #vbreus user with live viewer permissions able to modify web page item due to absence of
// action::Manager instance within QnResourceTreeModel instance. Need to figure out how to
// overcome this.
TEST_F(ResourceTreeModelTest, DISABLED_webPageItemIsEditableOnlyByAdmin)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    addWebPage(webPageName);

    // Perform actions and checks.
    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            hasFlag(Qt::ItemIsEditable))));

    loginAsLiveViewer("live_viewer");
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            noneOf(hasFlag(Qt::ItemIsEditable)))));
}

// TODO: #vbreus setData() is no-op due to absence of action::Manager instance
// within QnResourceTreeModel instance. Need to figure out how to overcome this.
TEST_F(ResourceTreeModelTest, DISABLED_webPageEditActionAffectsResourceName)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";
    static constexpr auto newPageName = "renamed_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    auto webPage = addWebPage(webPageName);
    auto index = uniqueMatchingIndex(displayFullMatch(webPageName));

    // Perform actions and checks.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));

    model()->setData(index, newPageName);
    ASSERT_TRUE(noneMatches(displayFullMatch(webPageName)));
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(newPageName)));
}

TEST_F(ResourceTreeModelTest, webPageItemIsDragEnabled)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(webPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            hasFlag(Qt::ItemIsDragEnabled))));
}

// TODO: #vbreus Doesn't pass. Not critical, but makes sense for the goodness.
TEST_F(ResourceTreeModelTest, DISABLED_webPageHasItemNeverHasChildrenFlag)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(webPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            hasFlag(Qt::ItemNeverHasChildren))));
}

// TODO: #vbreus Doesn't pass. Not critical, but causes misleading item hover decoration.
TEST_F(ResourceTreeModelTest, DISABLED_webPageItemIsNotDropEnabled)
{
    // String constants.
    static constexpr auto webPageName = "unique_web_page_name";

    // Set up environment.
    loginAsAdmin("admin");
    addWebPage(webPageName);

    // Check tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(webPageName)));
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            displayFullMatch(webPageName),
            noneOf(hasFlag(Qt::ItemIsDropEnabled)))));
}
