// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>
#include <client/client_globals.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueWebPageName = "unique_web_page_name";
static constexpr auto kUniqueServerName = "unique_server_name";
static constexpr auto kUniqueUserName = "unique_user_name";
static constexpr auto kServer1Name = "server_1";
static constexpr auto kServer2Name = "server_2";

// Predefined conditions.
static const auto kUniqueWebPageNameCondition = displayFullMatch(kUniqueWebPageName);
static const auto kUniqueServerNameCondition = displayFullMatch(kUniqueServerName);
static const auto kServer1NameCondition = displayFullMatch(kServer1Name);
static const auto kServer2NameCondition = displayFullMatch(kServer2Name);

TEST_F(ResourceTreeModelTest, webPageAdds)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueWebPageNameCondition));
}

TEST_F(ResourceTreeModelTest, webPageRemoves)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    auto webPage = addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueWebPageName)));

    // When previously added web page is removed from resource pool.
    resourcePool()->removeResource(webPage);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueWebPageNameCondition));
}

TEST_F(ResourceTreeModelTest, webPageIconTypes)
{
    // String constants.
    static constexpr auto kRegularWebPageName = "regular_web_page_name";
    static constexpr auto kClientApiWebPageName = "client_api_web_page_name";

    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with unique name and WebPageSubtype::none subtype added to the resource pool.
    const auto regularWebPage = addWebPage(kRegularWebPageName);
    regularWebPage->setSubtype(nx::vms::api::WebPageSubtype::none);

    // When web page with unique name and Client API subtype added to the resource pool.
    const auto clientApiWebPage = addWebPage(kClientApiWebPageName);
    clientApiWebPage->setSubtype(nx::vms::api::WebPageSubtype::clientApi);

    // Then exactly one node with plain web page display text appears in the resource tree.
    const auto regularWebPageIndex = uniqueMatchingIndex(displayFullMatch(kRegularWebPageName));

    // And that node has plain web page icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::WebPage)(regularWebPageIndex));

    // Then exactly one node with Client API web page display text appears in the resource tree.
    const auto clientApiWebPageIndex = uniqueMatchingIndex(displayFullMatch(kClientApiWebPageName));

    // And that node still has web page icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::WebPage)(clientApiWebPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageIsChildOfCorrespondingTopLevelNode)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    auto webPage = addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node is child of "Web Pages" node.
    ASSERT_TRUE(directChildOf(webPagesNodeCondition())(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageIsDisplayedToAnyUser)
{
    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueWebPageName)));

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueWebPageName)));
}

TEST_F(ResourceTreeModelTest, webPageIsNotDisplayedIfNotLoggedIn)
{
    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // When user is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUniqueWebPageName)));

    // When no user is logged in.
    logout();

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueWebPageName)));

    // Then no more nodes with with any possible web page icon found in the resource tree.
    ASSERT_TRUE(noneMatches(iconTypeMatch(QnResourceIconCache::WebPage)));
}

TEST_F(ResourceTreeModelTest, webPageHelpTopic)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, Qn::MainWindow_Tree_WebPage_Help)(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageTooltip)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node have same tooltip text as display text.
    ASSERT_TRUE(dataMatch(Qt::ToolTipRole, kUniqueWebPageName)(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageDisplayNameMapping)
{
    // When web page with certain unique name is added to the resource pool.
    auto webPage = addWebPage(kUniqueWebPageName);

    // When user is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node have same display text as QnWebPageResource::getName() returns.
    ASSERT_EQ(webPage->getName(), webPageIndex.data().toString());
}

TEST_F(ResourceTreeModelTest, webPageItemIsEditableOnlyByAdmin)
{
    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with corresponding display text appears in the resource tree.
    // And that node is can be edited.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(uniqueMatchingIndex(kUniqueWebPageNameCondition)));

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then exactly one node with corresponding display text appears in the resource tree.
    // And that node is cannot be edited.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(uniqueMatchingIndex(kUniqueWebPageNameCondition)));
}

TEST_F(ResourceTreeModelTest, webPageItemIsDragEnabled)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageHasItemNeverHasChildrenFlag)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node is declared as leaf node.
    ASSERT_TRUE(hasFlag(Qt::ItemNeverHasChildren)(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageItemIsNotDropEnabled)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When web page with certain unique name is added to the resource pool.
    addWebPage(kUniqueWebPageName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node isn't accept drop action.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDropEnabled)(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageEnableProxyThroughServer)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When single server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When web page with certain unique name is added to the resource pool.
    auto page = addWebPage(kUniqueWebPageName);

    // And web page is proxied through the server.
    page->setProxyId(server->getId());

    // Then exactly one node with corresponding page name text appears in the resource tree.
    auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And it is the direct child of the server.
    ASSERT_TRUE(directChildOf(serverIndex)(uniqueMatchingIndex(kUniqueWebPageNameCondition)));
}

TEST_F(ResourceTreeModelTest, proxiedWebPageSharedWithCustomUser)
{
    // When custom user with unique name is added to the resource pool.
    const auto customUser = addUser(kUniqueUserName, GlobalPermission::customUser);

    // When user is logged in.
    loginAsAdmin("admin");

    // When single server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When web page with certain unique name is added to the resource pool.
    auto page = addWebPage(kUniqueWebPageName);

    // And web page is proxied through the server.
    page->setProxyId(server->getId());

    // When proxied web page is shared to the custom user.
    setupAccessToResourceForUser(customUser, page, true);

    // Then exactly one node with corresponding proxied web page display text appears as child of
    // user's shared resources group.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            kUniqueWebPageNameCondition,
            directChildOf(sharedResourcesNodeCondition()))));
}

TEST_F(ResourceTreeModelTest, webPageDisableProxy)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When single server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When proxied web resource with certain unique name is added to the resource pool.
    auto page = addProxiedWebResource(kUniqueWebPageName, server->getId());

    // And proxy is disabled.
    page->setProxyId(QnUuid());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);

    // And that node is child of "Web Pages" node.
    ASSERT_TRUE(directChildOf(webPagesNodeCondition())(webPageIndex));
}

TEST_F(ResourceTreeModelTest, webPageMovesBetweenServers)
{
    // Given a system with two servers.
    const auto server1 = addServer(kServer1Name);
    const auto server2 = addServer(kServer2Name);

    // Given a proxied web page, which is the child of server 1.
    auto proxiedWebPage = addProxiedWebResource(kUniqueWebPageName, server1->getId());

    // When user is logged in.
    loginAsAdmin("admin");

    // Then there is exactly one web page in the Resource Tree and that node is a child of server 1.
    auto webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);
    ASSERT_TRUE(kServer1NameCondition(webPageIndex.parent()));

    // When server 2 is set as parent of proxied web page.
    proxiedWebPage->setParentId(server2->getId());

    // Then there is exactly one web page in the Resource Tree and that node is a child of server 2.
    webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);
    ASSERT_TRUE(kServer2NameCondition(webPageIndex.parent()));

    // When server 1 is set as parent of proxied web page back.
    proxiedWebPage->setParentId(server1->getId());

    // Then there is exactly one web page in the Resource Tree and that node is a child of server 1.
    webPageIndex = uniqueMatchingIndex(kUniqueWebPageNameCondition);
    ASSERT_TRUE(kServer1NameCondition(webPageIndex.parent()));
}

} // namespace test
} // namespace nx::vms::client::desktop
