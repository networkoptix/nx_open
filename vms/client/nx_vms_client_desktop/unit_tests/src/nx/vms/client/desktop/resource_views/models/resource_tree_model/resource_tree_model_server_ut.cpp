// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueServerName = "unique_server_name";
static constexpr auto kUniqueEdgeServerName = "unique_edge_server_name";

static constexpr auto kUniqueCameraName = "unique_camera_name";
static constexpr auto kUniqueEdgeCameraName = "unique_edge_camera_name";
static constexpr auto kUniqueVirtualCameraName = "unique_virtual_camera_name";
static constexpr auto kUniqueGroupName = "unique_group_name";

static constexpr auto kRenamedEdgeCameraName = "renamed_edge_camera_name";

static constexpr auto kValidIpV4Address = "192.168.0.0";

// Predefined conditions.
static const auto kUniqueServerNameCondition = displayFullMatch(kUniqueServerName);
static const auto kUniqueEdgeServerNameCondition = displayFullMatch(kUniqueEdgeServerName);

static const auto kUniqueCameraNameCondition = displayFullMatch(kUniqueCameraName);
static const auto kUniqueEdgeCameraNameCondition = displayFullMatch(kUniqueEdgeCameraName);
static const auto kUniqueVirtualCameraNameCondition = displayFullMatch(kUniqueVirtualCameraName);
static const auto kUniqueGroupNameCondition = displayFullMatch(kUniqueGroupName);

TEST_F(ResourceTreeModelTest, serverAdds)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueServerNameCondition));
}

TEST_F(ResourceTreeModelTest, serverRemoves)
{
    // When server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueServerNameCondition));

    // When previously added server resource is removed from resource pool.
    resourcePool()->removeResource(server);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));
}

TEST_F(ResourceTreeModelTest, serverIsNotDisplayedWhenNotLoggedIn)
{
    // When no one is logged in.

    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // Then no nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));
}

TEST_F(ResourceTreeModelTest, serverIsDisplayedAsServerForLiveViewer)
{
    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And it have server icon type.
    ASSERT_TRUE(serverIconTypeCondition()(serverIndex));
}

TEST_F(ResourceTreeModelTest, serverIconType)
{
    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And it have server icon type.
    ASSERT_TRUE(serverIconTypeCondition()(serverIndex));
}

TEST_F(ResourceTreeModelTest, serverIconStatus)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When Offline status is set to the server resource.
    server->setStatus(nx::vms::api::ResourceStatus::offline);

    // Then server icon has Offline decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Offline)(
        uniqueMatchingIndex(kUniqueServerNameCondition)));

    // When server is offline, it is marked as incompatible.
    server->setCompatible(false);

    // Then server icon has Incompatible decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Incompatible)(
        uniqueMatchingIndex(kUniqueServerNameCondition)));

    // When Unauthorized status is set to the server resource.
    server->setStatus(nx::vms::api::ResourceStatus::unauthorized);

    // Then server icon has Unauthorized decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Unauthorized)(
        uniqueMatchingIndex(kUniqueServerNameCondition)));

    // When Incompatible status is set to the server resource.
    server->setStatus(nx::vms::api::ResourceStatus::incompatible);

    // Then server icon has Incompatible decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Incompatible)(
        uniqueMatchingIndex(kUniqueServerNameCondition)));

    // When Online status is set to the server resource.
    server->setStatus(nx::vms::api::ResourceStatus::online);

    // Then server icon has Online decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Online)(
        uniqueMatchingIndex(kUniqueServerNameCondition)));
}

// TODO: #sivanov To correctly test this behavior we should emulate established connection, so for
// example create mock message processor and use it's remote guid here.
TEST_F(ResourceTreeModelTest, DISABLED_serverControlIconStatus)
{
    // When server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When server is online and remote GUID is server ID.
    server->setStatus(nx::vms::api::ResourceStatus::online);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then server icon has Control decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Control)
        (uniqueMatchingIndex(kUniqueServerNameCondition)));
}

TEST_F(ResourceTreeModelTest, reducedEdgeCameraIsDisplayedForAdmin)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then single reduced EDGE camera node found in the resource tree.
    const auto edgeCameraIndex = uniqueMatchingIndex(reducedEdgeCameraCondition());

    // And that node has corresponding display text.
    ASSERT_TRUE(kUniqueEdgeCameraNameCondition(edgeCameraIndex));

    // And no plain server nodes found in the resource tree.
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, reducedEdgeCameraIsDisplayedForLiveViewer)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // Then single reduced EDGE camera node found in the resource tree.
    const auto edgeCameraIndex = uniqueMatchingIndex(reducedEdgeCameraCondition());

    // And that node has corresponding display text.
    ASSERT_TRUE(kUniqueEdgeCameraNameCondition(edgeCameraIndex));

    // And no plain server nodes found in the resource tree.
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, reducedEdgeCameraIsDisplayedForAdvancedViewer)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // When user with advanced viewer permissions is logged in.
    loginAsAdvancedViewer("advanced_viewer");

    // Then single reduced EDGE camera node found in the resource tree.
    const auto edgeCameraIndex = uniqueMatchingIndex(reducedEdgeCameraCondition());

    // And that node has corresponding display text.
    ASSERT_TRUE(kUniqueEdgeCameraNameCondition(edgeCameraIndex));

    // And no plain server nodes found in the resource tree.
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, reducedEdgeCameraIsDisplayedForCustomUser)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // When access to both EDGE server and EDGE camera is granted to the custom user.
    setupAccessToResourceForUser(customUser, edgeServer, /*isAccessible*/ true);
    setupAccessToResourceForUser(customUser, edgeCamera, /*isAccessible*/ true);

    // Then single reduced EDGE camera node found in the resource tree.
    const auto edgeCameraIndex = uniqueMatchingIndex(reducedEdgeCameraCondition());

    // And that node has corresponding display text.
    ASSERT_TRUE(kUniqueEdgeCameraNameCondition(edgeCameraIndex));

    // And no plain server nodes found in the resource tree.
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, edgeServerIsExpandedIfRedundancySetOn)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // Then there are no reduced EDGE camera nodes in the tree.
    ASSERT_TRUE(noneMatches(reducedEdgeCameraCondition()));

    // Then exactly one camera node with corresponding display text founds in the resource tree.
    const auto edgeCameraIndex = uniqueMatchingIndex(
        allOf(kUniqueEdgeCameraNameCondition, cameraIconTypeCondition()));

    // And this node is the child of an server node.
    ASSERT_TRUE(directChildOf(serverIconTypeCondition())(edgeCameraIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerIsExpandedIfContainsSingleForeignCamera)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When foreign camera with certain unique name is added to the EDGE server.
    const auto camera = addCamera(kUniqueCameraName, edgeServer->getId());
    EXPECT_EQ(camera->getUrl(), QString());

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(false);

    // Then there are no reduced EDGE camera nodes in the tree.
    ASSERT_TRUE(noneMatches(reducedEdgeCameraCondition()));

    // Then exactly one camera node with corresponding display text founds in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(
        allOf(kUniqueCameraNameCondition, cameraIconTypeCondition()));

    // And this node is the child of an server node.
    ASSERT_TRUE(directChildOf(serverIconTypeCondition())(cameraIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerIsExpandedIfChildVirtualCameraAdded)
{
    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When EDGE server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then there is single reduced EDGE camera node in the resource tree.
    ASSERT_TRUE(onlyOneMatches(reducedEdgeCameraCondition()));
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));

    // When virtual camera is added to the mentioned EDGE server.
    const auto virtualCamera = addVirtualCamera(kUniqueVirtualCameraName, edgeServer->getId());

    // Then there are no reduced EDGE camera nodes in the tree.
    ASSERT_TRUE(noneMatches(reducedEdgeCameraCondition()));

    // Then there is single server node in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // Then two cameras are displayed as children of server node.
    ASSERT_TRUE(onlyOneMatches(allOf(
        directChildOf(serverIndex), kUniqueEdgeCameraNameCondition)));

    ASSERT_TRUE(onlyOneMatches(allOf(
        directChildOf(serverIndex), kUniqueVirtualCameraNameCondition)));

    // When previously added virtual camera is removed from the resource pool.
    resourcePool()->removeResource(virtualCamera);

    // Then there is single reduced EDGE camera node in the resource tree again.
    ASSERT_TRUE(onlyOneMatches(reducedEdgeCameraCondition()));
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, emptyEdgeServerAppearsIfEdgeCameraWasMovedToAnotherServer)
{
    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When EDGE server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then there are single reduced EDGE camera node and single server node in the resource tree.
    ASSERT_TRUE(onlyOneMatches(reducedEdgeCameraCondition()));
    ASSERT_TRUE(onlyOneMatches(anyOf(serverIconTypeCondition(), kUniqueServerNameCondition)));

    // When EDGE camera is moved to a foreign server.
    edgeCamera->setParentId(server->getId());

    // Then there are no reduced EDGE camera nodes in the tree.
    ASSERT_TRUE(noneMatches(reducedEdgeCameraCondition()));

    // Then there are two servers with different names in the resource tree.
    const auto plainServerIndex = uniqueMatchingIndex(kUniqueServerNameCondition);
    const auto edgeServerIndex = uniqueMatchingIndex(
        allOf(
            anyOf(kUniqueEdgeServerNameCondition, kUniqueEdgeCameraNameCondition),
            serverIconTypeCondition()));

    // Then EDGE server has no children.
    ASSERT_FALSE(hasChildren()(edgeServerIndex));

    // Then second server has child camera.
    ASSERT_TRUE(hasChildren()(plainServerIndex));
}

TEST_F(ResourceTreeModelTest, serverIsDisplayedForACustomUserIfUserGetsAccessToIt)
{
    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // Then no nodes with server name are found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));

    // When access to the server is granted to the custom user.
    setupAccessToResourceForUser(customUser, server, true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And it have server icon type.
    ASSERT_TRUE(serverIconTypeCondition()(serverIndex));

    // When access to the server is taken from the custom user.
    setupAccessToResourceForUser(customUser, server, false);

    // Then no nodes with server name are found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));
}

TEST_F(ResourceTreeModelTest, serverIsDisplayedForACustomUserIfUserGetsAccessToAChildCamera)
{
    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // Then no nodes with server or camera name are found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));
    ASSERT_TRUE(noneMatches(kUniqueCameraNameCondition));

    // When access to the camera is granted to the custom user.
    setupAccessToResourceForUser(customUser, camera, true);

    // Then exactly one node with corresponding camera display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it's a child of corresponding server node.
    ASSERT_TRUE(directChildOf(kUniqueServerNameCondition)(cameraIndex));

    // When access to the camera is taken from the custom user.
    setupAccessToResourceForUser(customUser, camera, false);

    // Then no nodes with server or camera name are found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));
    ASSERT_TRUE(noneMatches(kUniqueCameraNameCondition));
}

TEST_F(ResourceTreeModelTest, serverIsNotDisplayedForACustomUserIfAccessibleChildCameraRemoved)
{
    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // When access to the camera is granted to the custom user.
    setupAccessToResourceForUser(customUser, camera, true);

    // Then exactly one node with corresponding camera display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is a child of corresponding server node.
    ASSERT_TRUE(directChildOf(kUniqueServerNameCondition)(cameraIndex));

    // When previously added camera is removed from the resource pool.
    resourcePool()->removeResource(camera);

    // Then no nodes with server or camera name are found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueServerNameCondition));
    ASSERT_TRUE(noneMatches(kUniqueCameraNameCondition));
}

TEST_F(ResourceTreeModelTest, accessToServerIsChangedForACustomUserIfAccessibleChildCameraMoved)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When two Servers with different unique names are added to the resource pool.
    const auto server1 = addServer(kUniqueServerName1);
    const auto server2 = addServer(kUniqueServerName2);

    // When camera with certain unique name is added to the first server.
    const auto camera = addCamera(kUniqueCameraName, server1->getId());

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // When access to the camera is granted to the custom user.
    setupAccessToResourceForUser(customUser, camera, true);

    // Then exactly one node with corresponding camera display text appears in the resource tree.
    auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is a child of first server node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueServerName1))(cameraIndex));

    // When previously added camera is moved to the second server.
    camera->setParentId(server2->getId());

    // Then exactly one node with corresponding camera display text appears in the resource tree.
    cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is a child of second server node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueServerName2))(cameraIndex));

    // First server is no longer in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueServerName1)));
}

TEST_F(ResourceTreeModelTest, accessToServerIsChangedForACustomUserIfAccessibleChildRecorderMoved)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When two Servers with different unique names are added to the resource pool.
    const auto server1 = addServer(kUniqueServerName1);
    const auto server2 = addServer(kUniqueServerName2);

    // When recorder camera with certain unique name and group name is added to the first server.
    const auto recorderCamera = addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server1->getId());

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // When access to the recorder camera is granted to the custom user.
    setupAccessToResourceForUser(customUser, recorderCamera, true);

    // Then exactly one node with corresponding group name text appears in the resource tree.
    auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it have child accessible recorder camera.
    ASSERT_TRUE(directChildOf(recorderIndex)(uniqueMatchingIndex(kUniqueCameraNameCondition)));

    // And that node is a child of first server node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueServerName1))(recorderIndex));

    // When previously added recorder camera is moved to the second server.
    recorderCamera->setParentId(server2->getId());

    // Then exactly one node with corresponding group name text appears in the resource tree.
    recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And that node is a child of second server node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueServerName2))(recorderIndex));

    // And it have child accessible recorder camera.
    ASSERT_TRUE(directChildOf(recorderIndex)(uniqueMatchingIndex(kUniqueCameraNameCondition)));

    // First server is no longer in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueServerName1)));
}

TEST_F(ResourceTreeModelTest, hiddenEdgeServerIsDisplayedWhenCameraRemoved)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When camera resource with certain unique name is added to the server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // Then no server nodes found in the resource tree.
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));

    // When previously added camera resource is removed from the Resource Pool.
    resourcePool()->removeResource(edgeCamera);

    // Then server node appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueEdgeServerNameCondition));
    ASSERT_TRUE(onlyOneMatches(serverIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, singleServerIsTopLevelNode)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When single server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And it's top level node.
    ASSERT_TRUE(topLevelNode()(serverIndex));
}

TEST_F(ResourceTreeModelTest, multipleServersAreChildrenOfServersNodeForAdmin)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When two Servers with different unique names are added to the resource pool.
    addServer(kUniqueServerName1);
    addServer(kUniqueServerName2);

    // Then exactly two nodes with corresponding display texts appear in the resource tree.
    const auto server1Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName1));
    const auto server2Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName2));

    // And these nodes are children of "Servers" node.
    ASSERT_TRUE(directChildOf(serversNodeCondition())(server1Index));
    ASSERT_TRUE(directChildOf(serversNodeCondition())(server2Index));
}

TEST_F(ResourceTreeModelTest, multipleServersAreChildrenOfServersNodeForLiveViewer)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When user with advanced viewer permissions is logged in.
    loginAsLiveViewer("live_viewer");

    // When two empty servers with different unique names are added to the resource pool.
    addServer(kUniqueServerName1);
    addServer(kUniqueServerName2);

    // Then exactly two nodes with corresponding display texts appear in the resource tree.
    const auto server1Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName1));
    const auto server2Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName2));

    // And these nodes are children of "Servers" node.
    ASSERT_TRUE(directChildOf(serversNodeCondition())(server1Index));
    ASSERT_TRUE(directChildOf(serversNodeCondition())(server2Index));
}

TEST_F(ResourceTreeModelTest, multipleServersAreChildrenOfServersNodeForAdvancedViewer)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When two empty servers with different unique names are added to the resource pool.
    addServer(kUniqueServerName1);
    addServer(kUniqueServerName2);

    // When user with advanced viewer permissions is logged in.
    loginAsAdvancedViewer("advanced_viewer");

    // Then exactly two nodes with corresponding display texts appear in the resource tree.
    const auto server1Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName1));
    const auto server2Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName2));

    // And these nodes are children of "Servers" node.
    ASSERT_TRUE(directChildOf(serversNodeCondition())(server1Index));
    ASSERT_TRUE(directChildOf(serversNodeCondition())(server2Index));
}

TEST_F(ResourceTreeModelTest, serversBecameGroupedIfCustomUserGetsAccessToSecondServer)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When two servers with certain unique names are added to the resource pool.
    const auto server1 = addServer(kUniqueServerName1);
    const auto server2 = addServer(kUniqueServerName2);

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // When access to the first server is granted to the custom user.
    setupAccessToResourceForUser(customUser, server1, /*isAccessible*/ true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto server1Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName1));

    // And this node is top level node
    ASSERT_TRUE(topLevelNode()(server1Index));

    // When access to the second server is granted to the custom user.
    setupAccessToResourceForUser(customUser, server2, /*isAccessible*/ true);

    // Then all servers are grouped under "Servers" node.
    ASSERT_TRUE(noneMatches(allOf(serverIconTypeCondition(), topLevelNode())));
    ASSERT_EQ(2, matchCount(
        allOf(serverIconTypeCondition(), directChildOf(serversNodeCondition()))));

    // When access to the first server is taken from the custom user.
    setupAccessToResourceForUser(customUser, server1, /*isAccessible*/ false);

    // Then exactly one server node remains in the resource tree.
    const auto remainServerIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have second server display text.
    ASSERT_TRUE(displayFullMatch(kUniqueServerName2)(remainServerIndex));

    // And that node is top level node.
    ASSERT_TRUE(topLevelNode()(remainServerIndex));
}

TEST_F(ResourceTreeModelTest, serversBecameGroupedIfCustomUserGetsAccessToCameraOnSecondServer)
{
    // String constants.
    static constexpr auto kUniqueServerName1 = "unique_server_name_1";
    static constexpr auto kUniqueServerName2 = "unique_server_name_2";

    // When two servers with certain unique names are added to the resource pool.
    const auto server1 = addServer(kUniqueServerName1);
    const auto server2 = addServer(kUniqueServerName2);

    // When camera with certain unique name is added to the second server.
    const auto cameraOnServer2 = addCamera(kUniqueCameraName, server2->getId());

    // When custom user is logged in.
    const auto customUser = loginAsCustomUser("custom_user");

    // When access to the first server is granted to the custom user.
    setupAccessToResourceForUser(customUser, server1, /*isAccessible*/ true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto server1Index = uniqueMatchingIndex(displayFullMatch(kUniqueServerName1));

    // And this node is top level node
    ASSERT_TRUE(topLevelNode()(server1Index));

    // When access to the camera on the second server is granted to the custom user.
    setupAccessToResourceForUser(customUser, cameraOnServer2, /*isAccessible*/ true);

    // Then all servers are grouped under "Servers" node.
    ASSERT_TRUE(noneMatches(allOf(serverIconTypeCondition(), topLevelNode())));
    ASSERT_EQ(2, matchCount(
        allOf(serverIconTypeCondition(), directChildOf(serversNodeCondition()))));

    // When access to the camera on the second server is taken from the custom user.
    setupAccessToResourceForUser(customUser, cameraOnServer2, /*isAccessible*/ false);

    // Then exactly one server node remains in the resource tree.
    const auto remainServerIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have first server display text.
    ASSERT_TRUE(displayFullMatch(kUniqueServerName1)(remainServerIndex));

    // And that node is top level node.
    ASSERT_TRUE(topLevelNode()(remainServerIndex));
}

TEST_F(ResourceTreeModelTest, serverHelpTopic)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, HelpTopic::Id::MainWindow_Tree_Servers)(serverIndex));
}

TEST_F(ResourceTreeModelTest, serverDisplayNameMapping)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And that node have same display text as QnMediaServerResource::getName() returns.
    ASSERT_EQ(serverIndex.data().toString(), server->getName());
}

TEST_F(ResourceTreeModelTest, emptyEdgeServerDisplaysServerName)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have initial EDGE server name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithBindedCameraDisplaysBindedCameraName)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeCameraName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithBindedCameraRenamedWhenBindedCameraRenamed)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // When EDGE camera renamed.
    edgeCamera->setName(kRenamedEdgeCameraName);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have new binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kRenamedEdgeCameraName)(serverIndex));
    ASSERT_EQ(edgeCamera->getUserDefinedName(), edgeServer->getName());
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysServerNameIfBindedCameraRenamedOnForeignServer)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // When binded EDGE camera is moved to the foreign server.
    edgeCamera->setParentId(server->getId());

    // When EDGE camera renamed.
    edgeCamera->setName(kRenamedEdgeCameraName);

    // Then exactly one server with no children founds in the resource tree.
    const auto serverIndex =
        uniqueMatchingIndex(allOf(serverIconTypeCondition(), hasNoChildren()));

    // And that node have new binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysServerNameWhenBindedCameraRemoved)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When binded EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // When binded EDGE camera removed from the resource pool.
    resourcePool()->removeResource(edgeCamera);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have initial EDGE server name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysServerNameWhenMultipleResourcesRemoved)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When binded EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    const auto camera = addCamera("test camera");

    // Server gets EDGE camera name.
    ASSERT_EQ(kUniqueEdgeCameraName, edgeServer->getName());

    QObject::connect(resourcePool(), &QnResourcePool::resourceRemoved, resourcePool(),
        [&](const QnResourcePtr& resource)
        {
            // Resources are removed from pool before this signal is emitted.
            ASSERT_EQ(kUniqueEdgeServerName, edgeServer->getName());
        });

    // When binded EDGE camera removed from the resource pool.
    resourcePool()->removeResources({ camera, edgeCamera });

    // Server name returns back to original value.
    ASSERT_EQ(kUniqueEdgeServerName, edgeServer->getName());
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysServerNameWhenBindedCameraMovedToForeignServer)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When binded EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When binded EDGE camera is moved to the foreign server.
    edgeCamera->setParentId(server->getId());

    // Then exactly one server with no children founds in the resource tree.
    const auto serverIndex =
        uniqueMatchingIndex(allOf(serverIconTypeCondition(), hasNoChildren()));

    // And that node have initial EDGE server name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysBindedCameraNameIfCameraMovedFromForeignServer)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When EDGE camera with certain unique name is added to the foreign server.
    const auto edgeCamera = addCamera(kUniqueEdgeCameraName, server->getId(), kValidIpV4Address);

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When foreign camera is added to the EDGE server.
    const auto camera = addCamera(kUniqueCameraName, edgeServer->getId());

    // When binded EDGE camera moved from foreign server to EDGE server.
    edgeCamera->setParentId(edgeServer->getId());

    // Then exactly one server with children founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(allOf(serverIconTypeCondition(), hasChildren()));

    // And that node have binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeCameraName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithBindedCameraAndForeignCameraDisplaysBindedCameraName)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When foreign camera with certain unique name is added to the EDGE server.
    addCamera(kUniqueCameraName, edgeServer->getId());

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeCameraName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithBindedCameraAndVirtualCameraDisplaysBindedCameraName)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When virtual camera with certain unique name is added to the EDGE server.
    addVirtualCamera(kUniqueVirtualCameraName, edgeServer->getId());

    // When EDGE camera with certain unique name is added to the EDGE server.
    addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeCameraName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithSingleForeignCameraDisplaysServerName)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When foreign camera with certain unique name is added to the EDGE server.
    addCamera(kUniqueCameraName, edgeServer->getId());

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // Then exactly one server node founds in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(serverIconTypeCondition());

    // And that node have initial EDGE server name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysServerNameIfBindedCameraIsOnForeignServer)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When EDGE camera with certain unique name is added to the foreign server.
    const auto edgeCamera = addCamera(kUniqueEdgeCameraName, server->getId(), kValidIpV4Address);

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // Then exactly one server with no children founds in the resource tree.
    const auto serverIndex =
        uniqueMatchingIndex(allOf(serverIconTypeCondition(), hasNoChildren()));

    // And that node have initial EDGE server name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithDuplicatedBindedCamerasAppearsExpandedAndDisplaysServerName)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When EDGE camera with certain unique name is added to EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When duplicated EDGE camera with the same name is added to EDGE server.
    const auto edgeCamera2 = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // Then exactly one server node with two child nodes founds in the resource tree.
    const auto serverIndex =
        uniqueMatchingIndex(allOf(serverIconTypeCondition(), hasExactChildrenCount(2)));

    // And that node have initial EDGE server name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));

    // When EDGE cameras renamed.
    edgeCamera->setName(kRenamedEdgeCameraName);
    edgeCamera2->setName(kRenamedEdgeCameraName);

    // Then server name isn't affected.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeServerName)(serverIndex));

    // Then camera nodes are renamed as expected.
    ASSERT_EQ(2, matchCount(allOf(
        cameraIconTypeCondition(),
        displayFullMatch(kRenamedEdgeCameraName),
        directChildOf(serverIndex))));
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysBindedCameraNameIfDuplicateBindedCameraRemoved)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // When EDGE camera with certain unique name is added to EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When duplicated EDGE camera with the same name is added to EDGE server.
    const auto edgeCamera2 = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When one of duplicated EDGE cameras is removed from the resource pool.
    resourcePool()->removeResource(edgeCamera);

    // Then exactly one server node with one child node founds in the resource tree.
    const auto serverIndex =
        uniqueMatchingIndex(allOf(serverIconTypeCondition(), hasExactChildrenCount(1)));

    // And that node have binded EDGE camera name as display text.
    ASSERT_TRUE(displayFullMatch(kUniqueEdgeCameraName)(serverIndex));

    // When server redundancy flag set to false.
    edgeServer->setRedundancy(false);

    // Then EDGE server appears collapsed as it should be.
    ASSERT_TRUE(noneMatches(serverIconTypeCondition()));
    ASSERT_TRUE(onlyOneMatches(allOf(cameraIconTypeCondition(), hasNoChildren(), topLevelNode())));
}

TEST_F(ResourceTreeModelTest, edgeServerDisplaysBindedCameraNameIfDuplicateBindedCameraMovedToForeignServer)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When server redundancy flag set to true.
    edgeServer->setRedundancy(true);

    // When EDGE camera with certain unique name is added to EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When duplicated EDGE camera with the same name is added to EDGE server.
    const auto edgeCamera2 = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When another server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When one of duplicated EDGE cameras is moved to the foreign server.
    edgeCamera2->setParentId(server->getId());

    // Then exactly one server node with single child that have EDGE camera name founds in the
    // resource tree.
    uniqueMatchingIndex(allOf(
        serverIconTypeCondition(),
        hasExactChildrenCount(1),
        displayFullMatch(kUniqueEdgeCameraName)));

    // When duplicated EDGE camera on the foreign server is renamed.
    edgeCamera2->setName(kRenamedEdgeCameraName);

    // Then EDGE server node name isn't affected.
    uniqueMatchingIndex(allOf(
        serverIconTypeCondition(),
        hasExactChildrenCount(1),
        displayFullMatch(kUniqueEdgeCameraName)));

    // Then foreign server node name isn't affected, camera on the foreign server have
    // expected name.
    uniqueMatchingIndex(allOf(
        cameraIconTypeCondition(),
        hasNoChildren(),
        displayFullMatch(kRenamedEdgeCameraName),
        directChildOf(allOf(
            serverIconTypeCondition(),
            displayFullMatch(kUniqueServerName)))));
}

TEST_F(ResourceTreeModelTest,
    edgeServerExpandsWhenBindedCameraMovesToForeignServerAndCollapsesWhenBindedCameraMovesBack)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When binded EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When another server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // Then exactly one server node and exactly one camera node founds in the resource tree on the
    // same level under the "Servers" group.
    ASSERT_TRUE(onlyOneMatches(allOf(
        serverIconTypeCondition(), hasNoChildren(), directChildOf(serversNodeCondition()))));

    ASSERT_TRUE(onlyOneMatches(allOf(
        cameraIconTypeCondition(), hasNoChildren(), directChildOf(serversNodeCondition()))));

    ASSERT_EQ(2, matchCount(directChildOf(serversNodeCondition())));

    // When binded EDGE camera is moved to the foreign server.
    edgeCamera->setParentId(server->getId());

    // Then exactly two server nodes and exactly one camera node with foreign server parent
    // founds in the resource tree.
    ASSERT_EQ(2, matchCount(serverIconTypeCondition()));
    ASSERT_TRUE(onlyOneMatches(allOf(
        cameraIconTypeCondition(),
        directChildOf(displayFullMatch(kUniqueServerName)))));

    // When binded EDGE camera is moved back to the EDGE server.
    edgeCamera->setParentId(edgeServer->getId());

    // Then again exactly one server node and exactly one camera node founds in the resource tree
    // on the same level under the "Servers" group.
    ASSERT_TRUE(onlyOneMatches(allOf(
        serverIconTypeCondition(), hasNoChildren(), directChildOf(serversNodeCondition()))));

    ASSERT_TRUE(onlyOneMatches(allOf(
        cameraIconTypeCondition(), hasNoChildren(), directChildOf(serversNodeCondition()))));

    ASSERT_EQ(2, matchCount(directChildOf(serversNodeCondition())));
}

TEST_F(ResourceTreeModelTest, thereIsNoCrashWhenDuplicatedEdgeCamerasAreRemoved)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When EDGE server with certain unique name is added to the resource pool.
    const auto edgeServer = addEdgeServer(kUniqueEdgeServerName, kValidIpV4Address);

    // When binded EDGE camera with certain unique name is added to the EDGE server.
    const auto edgeCamera1 = addEdgeCamera(kUniqueEdgeCameraName, edgeServer);

    // When non-EDGE server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When duplicated EDGE camera with certain unique name is added to that foreign server.
    const auto edgeCamera2 = addCamera(kUniqueEdgeCameraName, server->getId(), kValidIpV4Address);

    // Then exactly one server node and exactly two camera nodes founds in the resource tree.
    ASSERT_EQ(1, matchCount(serverIconTypeCondition()));
    ASSERT_EQ(2, matchCount(cameraIconTypeCondition()));

    // When both EDGE cameras removed from the resource pool.
    resourcePool()->removeResources({edgeCamera1, edgeCamera2});

    // Then exactly two server nodes and no camera nodes founds in the resource tree.
    ASSERT_EQ(2, matchCount(serverIconTypeCondition()));
    ASSERT_EQ(0, matchCount(cameraIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, serverNodeProvidesResource)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And that node provides pointer to the server resource.
    ASSERT_TRUE(dataMatch(core::ResourceRole, QVariant::fromValue<QnResourcePtr>(server))(
        serverIndex));
}

TEST_F(ResourceTreeModelTest, serverNodeIsDragEnabled)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(serverIndex));
}

TEST_F(ResourceTreeModelTest, serverHasNotItemNeverHasChildrenFlag)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server resource with certain unique name is added to the resource pool.
    addServer(kUniqueServerName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto serverIndex = uniqueMatchingIndex(kUniqueServerNameCondition);

    // And that node is declared as node which may have children.
    ASSERT_FALSE(hasFlag(Qt::ItemNeverHasChildren)(serverIndex));
}

} // namespace test
} // namespace nx::vms::client::desktop
