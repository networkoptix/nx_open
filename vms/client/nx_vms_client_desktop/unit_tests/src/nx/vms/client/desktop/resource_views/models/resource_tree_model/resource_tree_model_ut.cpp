#include "resource_tree_model_test_fixture.h"

#include <core/resource/media_server_resource.h>
#include <ui/style/resource_icon_cache.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

// Temporary. Just proof that camera is displayed and nothing crashes.
TEST_F(ResourceTreeModelTest, cameraCheck)
{
    // String constants.
    static constexpr auto kCameraName = "fancy_camera";

    // Set up environment.
    loginAsAdmin("admin");
    const auto server = addServer("server");
    const auto serverId = server->getId();
    addCamera(kCameraName, serverId);

    // Check tree.
    ASSERT_TRUE(atLeastOneMatches(directChildOf(displayContains("server"))));
    ASSERT_TRUE(atLeastOneMatches(
        anyOf(
            displayContains(kCameraName),
            iconTypeMatch(QnResourceIconCache::Camera))));
}

// Temporary. Just proof that online server is displayed and nothing crashes.
TEST_F(ResourceTreeModelTest, onlineServerCheck)
{
    // String constants.
    static constexpr auto kServerName = "fancy_server";

    // Set up environment.
    loginAsAdmin("admin");
    const auto server = addServer(kServerName);
    server->setStatus(Qn::Online);

    ASSERT_TRUE(atLeastOneMatches(
        allOf(
            displayContains(kServerName),
            iconTypeMatch(QnResourceIconCache::Server),
            iconStatusMatch(QnResourceIconCache::Online))));
}
