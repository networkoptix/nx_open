// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/help/help_topics.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>
#include <client/client_globals.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

// String constants.
static constexpr auto kUniqueCameraName = "unique_camera_name";
static constexpr auto kUniqueGroupName = "unique_group_name";
static constexpr auto kRenamedCameraName = "renamed_camera_name";

// Predefined conditions.
static const auto kUniqueCameraNameCondition = displayFullMatch(kUniqueCameraName);
static const auto kUniqueGroupNameCondition = displayFullMatch(kUniqueGroupName);

TEST_F(ResourceTreeModelTest, cameraAdds)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding camera display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueCameraNameCondition));
}

TEST_F(ResourceTreeModelTest, cameraRemoves)
{
    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueCameraNameCondition));

    // When previously added camera resource is removed from resource pool.
    resourcePool()->removeResource(camera);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueCameraNameCondition));
}

TEST_F(ResourceTreeModelTest, cameraIconType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera resource with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it have camera icon type.
    iconTypeMatch(QnResourceIconCache::Camera)(cameraIndex);
}

TEST_F(ResourceTreeModelTest, ioModuleIconType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When IO Module with certain unique name is added to the server.
    addIOModule(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it have IO module icon type.
    iconTypeMatch(QnResourceIconCache::IOModule)(cameraIndex);
}

TEST_F(ResourceTreeModelTest, multisensorCameraIconType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When IO module with certain unique name is added to the server.
    addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto multisensorCameraIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it have multisensor camera icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::MultisensorCamera)(multisensorCameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderIconType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder with certain unique name is added to the server.
    addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it have recorder icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::Recorder)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderNameIsNotDefaultIfUserDefinedNameExists)
{
    static constexpr auto kDefaultGroupName = "default_group_name";
    static constexpr auto kUserGroupName = "user_group_name";

    QList<QnVirtualCameraResourcePtr> cameraList;

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When two recorder cameras with non-empty default group name are added to the server.
    cameraList.push_back(addRecorderCamera("camera", kUniqueGroupName, server->getId()));
    cameraList.push_back(addRecorderCamera("camera", kUniqueGroupName, server->getId()));
    for (const auto& camera: cameraList)
        camera->setDefaultGroupName(kDefaultGroupName);

    // When user defined group name is set to the first camera;
    cameraList.first()->setUserDefinedGroupName(kUserGroupName);

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // Then exactly one node with user group name display text appears in the resource tree and
    // there are no other group names appear in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUserGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kDefaultGroupName)));

    logout();

    // Order of cameras doesn't matter.
    cameraList.first()->setUserDefinedGroupName({});
    cameraList.last()->setUserDefinedGroupName(kUserGroupName);

    loginAsAdmin("admin");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUserGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kDefaultGroupName)));
}

TEST_F(ResourceTreeModelTest, virtualCameraIconType)
{
    // When User with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When virtual camera with certain unique name is added to the server.
    addVirtualCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto virtualCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it have virtual camera icon type.
    iconTypeMatch(QnResourceIconCache::VirtualCamera)(virtualCameraIndex);
}

TEST_F(ResourceTreeModelTest, cameraIconStatus)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When Offline status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::offline);

    // Then camera icon has Offline decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Offline)(
        uniqueMatchingIndex(kUniqueCameraNameCondition)));

    // When Unauthorized status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::unauthorized);

    // Then camera icon has Unauthorized decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Unauthorized)(
        uniqueMatchingIndex(kUniqueCameraNameCondition)));

    // When Incompatible status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::incompatible);

    // Then camera icon has Incompatible decoration.
    ASSERT_TRUE(iconStatusMatch(QnResourceIconCache::Incompatible)(
        uniqueMatchingIndex(kUniqueCameraNameCondition)));
}

TEST_F(ResourceTreeModelTest, cameraScheduledExtraStatus)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When setLicenseUsed flag set true for the camera resource.
    camera->setLicenseUsed(true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it provides scheduled extra status flag.
    ASSERT_TRUE(hasCameraExtraStatusFlag(CameraExtraStatusFlag::scheduled)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraBuggyExtraStatus)
{
    using namespace nx::vms::api;

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera resource with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // It has no buggy status by default.
    ASSERT_FALSE(hasCameraExtraStatusFlag(CameraExtraStatusFlag::buggy)(cameraIndex));

    // When issue occurred on the camera.
    camera->addStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag);

    // It provides buggy extra status flag.
    ASSERT_TRUE(hasCameraExtraStatusFlag(CameraExtraStatusFlag::buggy)(cameraIndex));

    // When schedule is marked as invalid.
    camera->addStatusFlags(CameraStatusFlag::CSF_InvalidScheduleFlag);

    // Then camera is still buggy.
    ASSERT_TRUE(hasCameraExtraStatusFlag(CameraExtraStatusFlag::buggy)(cameraIndex));

    // Even when camera issues stop.
    camera->removeStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag);

    // Then camera is still buggy.
    ASSERT_TRUE(hasCameraExtraStatusFlag(CameraExtraStatusFlag::buggy)(cameraIndex));

    // And when camera schedule is back to normal.
    camera->removeStatusFlags(CameraStatusFlag::CSF_InvalidScheduleFlag);

    // Then camera is not buggy anymore.
    ASSERT_FALSE(hasCameraExtraStatusFlag(CameraExtraStatusFlag::buggy)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsEditableByAdmin)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one camera node with corresponding display text appears in the tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsEditableByCustomUserWithEditCamerasPermissions)
{
    // When custom user with permission to edit camera settings is logged in.
    const auto user = loginAsUserWithPermissions("custom_with_edit_cameras",
        {GlobalPermission::customUser, GlobalPermission::editCameras});

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // And custom user gets access to that camera.
    setupAccessToResourceForUser(user, camera, true);

    // Then exactly one camera node with corresponding display text appears in the tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsNotEditableByPlainCustomUser)
{
    // When custom user with permission to edit camera settings is logged in.
    const auto user = loginAsUserWithPermissions("custom_with_edit_cameras",
        {GlobalPermission::customUser});

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // And custom user gets access to that camera.
    setupAccessToResourceForUser(user, camera, true);

    // Then exactly one camera node with corresponding display text appears in the tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsNotEditableByAdvancedViewer)
{
    // When advanced viewer is logged in.
    loginAsAdvancedViewer("advanced_viewer");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then exactly one camera node with corresponding display text appears in the tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderGroupIsEditableByAdmin)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    addRecorderCamera("recorder", kUniqueGroupName, server->getId());

    // Then exactly one recorder group node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderGroupIsEditableByCustomUserWithEditCamerasPermissions)
{
    // When custom user with permission to edit camera settings is logged in.
    const auto user = loginAsUserWithPermissions("custom_with_edit_cameras",
        {GlobalPermission::customUser, GlobalPermission::editCameras});

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    const auto recorderCamera = addRecorderCamera("recorder", kUniqueGroupName, server->getId());

    // And custom user gets access to that recorder camera.
    setupAccessToResourceForUser(user, recorderCamera, true);

    // Then exactly one recorder group node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it has editable flag.
    ASSERT_TRUE(hasFlag(Qt::ItemIsEditable)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderGroupIsNotEditableByPlainCustomUser)
{
    // When custom user with permission to edit camera settings is logged in.
    const auto user = loginAsUserWithPermissions("custom_with_edit_cameras",
        {GlobalPermission::customUser});

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    const auto recorderCamera = addRecorderCamera("recorder", kUniqueGroupName, server->getId());

    // And custom user gets access to that recorder camera.
    setupAccessToResourceForUser(user, recorderCamera, true);

    // Then exactly one recorder group node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderGroupIsNotEditableByAdvancedViewer)
{
    // When advanced viewer is logged in.
    loginAsAdvancedViewer("advanced_viewer");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    addRecorderCamera("recorder", kUniqueGroupName, server->getId());

    // Then exactly one recorder camera node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderIsGroupedInCustomUserAccessibleCamerasList)
{
    // When advanced viewer is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    const auto recorderCamera =
        addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // When custom user is added to the resource pool.
    const auto customUser = addUser("custom_user", {GlobalPermission::customUser});

    // And access to that recorder is granted to the custom user.
    setupAccessToResourceForUser(customUser, recorderCamera, true);

    // Then exactly two indexes matching with recorder camera name found in the tree.
    const auto cameraIndexes = allMatchingIndexes(kUniqueCameraNameCondition);
    ASSERT_EQ(cameraIndexes.size(), 2);

    // Then both are children of node with recorder group name.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueGroupName))(cameraIndexes.first()));
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueGroupName))(cameraIndexes.last()));

    // And access to recorder is taken from the custom user.
    setupAccessToResourceForUser(customUser, recorderCamera, false);

    // Then exactly one recorder camera node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);
}

TEST_F(ResourceTreeModelTest, singleCameraRecorderGroupExtraStatus)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with certain unique name is added to the server.
    const auto camera = addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one recorder camera node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it provides empty extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::empty)(recorderIndex));

    // When setLicenseUsed flag set true for the camera resource.
    camera->setLicenseUsed(true);

    // Then recorder node provides scheduled extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::scheduled)(recorderIndex));

    // When status changed to Recording.
    camera->setStatus(nx::vms::api::ResourceStatus::recording);

    // Then recorder node provides recording extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch({CameraExtraStatusFlag::scheduled,
        CameraExtraStatusFlag::recording})(recorderIndex));
}

TEST_F(ResourceTreeModelTest, twoCamerasRecorderGroupExtraStatus)
{
    // String constants
    static constexpr auto kUniqueCameraName1 = "unique_camera_name_1";
    static constexpr auto kUniqueCameraName2 = "unique_camera_name_2";

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When two recorder cameras with different unique names is added to the server.
    const auto camera1 = addRecorderCamera(kUniqueCameraName1, kUniqueGroupName, server->getId());
    const auto camera2 = addRecorderCamera(kUniqueCameraName2, kUniqueGroupName, server->getId());

    // Then there are two recorder cameras with corresponding names in the resource tree.
    const auto camera1Index = uniqueMatchingIndex(displayFullMatch(kUniqueCameraName1));
    const auto camera2Index = uniqueMatchingIndex(displayFullMatch(kUniqueCameraName2));

    // When setLicenseUsed flag set true for the second recorder camera.
    camera2->setLicenseUsed(true);

    // Then second recorder camera provides scheduled extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::scheduled)(camera2Index));

    // As well as parent recorder group node.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::scheduled)(
        camera2Index.parent()));

    // First recorder camera provides no extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::empty)(camera1Index));

    // When status of the second recorder camera changed to Recording.
    camera2->setStatus(nx::vms::api::ResourceStatus::recording);

    // Then second recorder camera also provides recording extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(
        {CameraExtraStatusFlag::scheduled, CameraExtraStatusFlag::recording})(
        camera2Index));

    // As well as parent recorder group node.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(
        {CameraExtraStatusFlag::scheduled, CameraExtraStatusFlag::recording})(
        camera2Index.parent()));

    // First recorder camera still provides no extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::empty)(camera1Index));
}

TEST_F(ResourceTreeModelTest, twoCamerasOnTwoServersRecorderGroupsExtraStatus)
{
    // String constants
    static constexpr auto kUniqueCameraName1 = "unique_camera_name_1";
    static constexpr auto kUniqueCameraName2 = "unique_camera_name_2";

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When two servers are added to the resource pool.
    const auto server1 = addServer("server_1");
    const auto server2 = addServer("server_2");

    // When two recorder cameras with different unique names is added to the different servers.
    const auto camera1 = addRecorderCamera(kUniqueCameraName1, kUniqueGroupName, server1->getId());
    const auto camera2 = addRecorderCamera(kUniqueCameraName2, kUniqueGroupName, server2->getId());

    // Then two nodes with corresponding display texts appears in the resource tree.
    const auto camera1Index = uniqueMatchingIndex(displayFullMatch(kUniqueCameraName1));
    const auto camera2Index = uniqueMatchingIndex(displayFullMatch(kUniqueCameraName2));

    // Then this nodes have different parents
    ASSERT_TRUE(camera1Index.parent() != camera2Index.parent());

    // When setLicenseUsed flag set true for the first recorder camera.
    camera1->setLicenseUsed(true);

    // Then it provides scheduled extra status flag.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::scheduled)(
        camera1Index));

    // As well as parent (recorder group) of the first recorder.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::scheduled)(
        camera1Index.parent()));

    // Second recorder camera provides no extra status.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::empty)(camera2Index));

    // As well as parent (recorder group) of the second recorder.
    ASSERT_TRUE(cameraExtraStatusFlagsMatch(CameraExtraStatusFlag::empty)(camera2Index.parent()));
}

TEST_F(ResourceTreeModelTest, cameraIsTopLevelNodeOnHiddenEdgeServer)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When edge server is added to the resource pool.
    const auto server = addEdgeServer("server", "192.168.0.0");

    // When edge camera with certain unique name is added to the server.
    const auto camera = addEdgeCamera(kUniqueCameraName, server);

    // When redundancy flag set to false.
    server->setRedundancy(false);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node have camera icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::Camera)(cameraIndex));

    // And that node is top level node.
    ASSERT_TRUE(topLevelNode()(cameraIndex));
}

TEST_F(ResourceTreeModelTest, reducedEdgeCameraNodeType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When edge server is added to the resource pool.
    const auto server = addEdgeServer("server", "192.168.0.0");

    // When edge camera with certain unique name is added to the server.
    const auto camera = addEdgeCamera(kUniqueCameraName, server);

    // When redundancy flag set to false.
    server->setRedundancy(false);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node have 'edge' node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::edge)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, expandedEdgeCameraNodeType)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When edge server is added to the resource pool.
    const auto server = addEdgeServer("server", "192.168.0.0");

    // When edge camera with certain unique name is added to the server.
    const auto camera = addEdgeCamera(kUniqueCameraName, server);

    // When redundancy flag set to false.
    server->setRedundancy(true);

    // Then exactly one camera node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(allOf(
        iconTypeMatch(QnResourceIconCache::Camera),
        kUniqueCameraNameCondition));

    // And that node have 'resource' node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::resource)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, edgeServerWithVirtualCameraIsNotHidden)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When edge server is added to the resource pool.
    const auto server = addEdgeServer("server", "192.168.0.0");

    // When edge camera is added to the server.
    const auto camera = addEdgeCamera("camera", server);

    // When virtual camera with certain unique name is added to the server.
    addVirtualCamera(kUniqueCameraName, server->getId());

    // When redundancy flag set to false.
    server->setRedundancy(false);

    // Then exactly one node with corresponding display text appear in the resource tree.
    const auto virtualCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node have virtual camera icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::VirtualCamera)(virtualCameraIndex));

    // And that node is child of the server node.
    ASSERT_TRUE(directChildOf(iconTypeMatch(QnResourceIconCache::Server))(virtualCameraIndex));
}

TEST_F(ResourceTreeModelTest, virtualCameraOnExpandedEdgeServerIsChildOfServerNodeForAdmin)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When edge server is added to the resource pool.
    const auto server = addEdgeServer("edge_server", "192.168.0.0");

    // When camera resource is added to the server.
    addEdgeCamera("edge_camera", server);

    // When virtual camera with certain unique name is added to the server.
    addVirtualCamera(kUniqueCameraName, server->getId());

    // When redundancy flag set to true.
    server->setRedundancy(true);

    // Then exactly one node with corresponding display text appear in the resource tree.
    const auto virtualCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node have virtual camera icon type.
    ASSERT_TRUE(iconTypeMatch(QnResourceIconCache::VirtualCamera)(virtualCameraIndex));

    // And that node is child of server node.
    ASSERT_TRUE(directChildOf(iconTypeMatch(QnResourceIconCache::Server))(virtualCameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsChildOfServerNodeForAdmin)
{
    // String constants.
    static constexpr auto kUniqueServerName = "unique_server_name";

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server with certain unique name is added to the resource pool.
    const auto server = addServer(kUniqueServerName);

    // When camera with certain unique name is added to the resource pool.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appear in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is child of server node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueServerName))(cameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderCameraIsChildOfRecorderNode)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with certain unique name is added to the server.
    addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto recorderCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is the child of recorder node.
    ASSERT_TRUE(directChildOf(kUniqueGroupNameCondition)(recorderCameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderCameraMovesToAnotherRecorder)
{
    // String constants
    static constexpr auto kUniqueGroupName1 = "unique_group_name_1";
    static constexpr auto kUniqueGroupName2 = "unique_group_name_2";

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // Given two different non-empty recorder nodes in the resource tree.
    addRecorderCamera("camera", kUniqueGroupName1, server->getId());
    addRecorderCamera("camera", kUniqueGroupName2, server->getId());

    // When recorder camera with unique name is added to the first recorder.
    auto camera = addRecorderCamera(kUniqueCameraName, kUniqueGroupName1, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is the child of the first recorder node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueGroupName1))(cameraIndex));

    // When previously mentioned recorder camera is moved to the second recorder.
    camera->setGroupId(kUniqueGroupName2);

    // Then exactly one node with corresponding display text appears in the resource tree.
    cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is the child of the first recorder node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueGroupName2))(cameraIndex));
}

TEST_F(ResourceTreeModelTest, singleRecorderCameraMovesToAnotherRecorder)
{
    // String constants
    static constexpr auto kUniqueGroupName1 = "unique_group_name_1";
    static constexpr auto kUniqueGroupName2 = "unique_group_name_2";

    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // Given two non-empty recorder in the resource tree, one contains camera with unique name.
    auto camera = addRecorderCamera(kUniqueCameraName, kUniqueGroupName1, server->getId());
    addRecorderCamera("camera", kUniqueGroupName2, server->getId());

    // When camera with unique name is moved to another recorder.
    camera->setGroupId(kUniqueGroupName2);

    // Then exactly one node with corresponding camera display text appears in the resource tree.
    auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is the child of the second recorder node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueGroupName2))(cameraIndex));

    // First recorder became empty and disappeared from the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueGroupName1)));
}

TEST_F(ResourceTreeModelTest, recorderGroupHidesWhenSingleRecorderCameraRemoved)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // Given single recorder in the resource tree which contains camera with unique name.
    auto camera = addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // When recorder camera with unique name is removed from the resource pool.
    resourcePool()->removeResource(camera);

    // Then there is no node with camera name in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueCameraNameCondition));

    // Then there is no node with recorder name in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueGroupNameCondition));
}

TEST_F(ResourceTreeModelTest, subCameraIsChildOfMultisensorCameraNode)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When multisensor sub-camera with certain unique name is added to the server.
    addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto multisensorSubCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is the child of multisensor camera node.
    ASSERT_TRUE(directChildOf(kUniqueGroupNameCondition)(multisensorSubCameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraHelpTopic)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, Qn::MainWindow_Tree_Camera_Help)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, ioModuleHelpTopic)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When IO module with certain unique name is added to the server.
    addIOModule(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto ioModuleIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, Qn::IOModules_Help)(ioModuleIndex));
}

TEST_F(ResourceTreeModelTest, recorderHelpTopic)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder with certain unique name is added to the server.
    addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And that node provides certain help topic data.
    ASSERT_TRUE(dataMatch(Qn::HelpTopicIdRole, Qn::MainWindow_Tree_Recorder_Help)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, cameraNodeProvidesResource)
{
    // When user with administrator permissions is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node provides pointer to the camera resource.
    ASSERT_TRUE(dataMatch(Qn::ResourceRole, QVariant::fromValue<QnResourcePtr>(camera))(
        cameraIndex));
}

// TODO: #vbreus setData() is no-op due to absence of action::Manager instance
// within QnResourceTreeModel instance.
TEST_F(ResourceTreeModelTest, resourceEditActionAffectsTree)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When camera is renamed.
    camera->setName(kRenamedCameraName);

    // Then camera resource have new name.
    ASSERT_EQ(camera->getUserDefinedName(), kRenamedCameraName);

    // Then exactly one node with new name appears in the model.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kRenamedCameraName)));

    // And no nodes with initial display text left in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueCameraName)));
}

TEST_F(ResourceTreeModelTest, recorderDisplayNameMapping)
{
    // String constants.
    static constexpr auto kUserDefinedGroupName = "unique_user_defined_group_name";

    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder with certain unique name is added to the server.
    const auto recorderCamera =
        addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // When user defined group name of the recorder camera is not specified.
    ASSERT_TRUE(recorderCamera->getUserDefinedGroupName().isNull());

    // Then recorder node have group ID as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            iconTypeMatch(QnResourceIconCache::Recorder),
            displayFullMatch(recorderCamera->getGroupId()))));

    // When user defined group name is set to the recorder camera.
    recorderCamera->setUserDefinedGroupName(kUserDefinedGroupName);

    // Then recorder node have user defined group name as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            iconTypeMatch(QnResourceIconCache::Recorder),
            displayFullMatch(kUserDefinedGroupName))));
}

TEST_F(ResourceTreeModelTest, recorderGroupRenamedWhenGroupNameSetToTheLastAddedCamera)
{
    // String constants.
    static constexpr auto kUserDefinedGroupName = "unique_user_defined_group_name";

    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When two recorder cameras are sequentially added to the server.
    const auto firstRecorderCamera =
        addRecorderCamera("first_recorder_camera", kUniqueGroupName, server->getId());

    const auto secondRecorderCamera =
        addRecorderCamera("second_recorder_camera", kUniqueGroupName, server->getId());

    // When first camera is removed from resource pool.
    resourcePool()->removeResource(firstRecorderCamera);

    // When user defined group name is set to the second recorder camera.
    secondRecorderCamera->setUserDefinedGroupName(kUserDefinedGroupName);

    // Then recorder node have user defined group name as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
        iconTypeMatch(QnResourceIconCache::Recorder),
        displayFullMatch(kUserDefinedGroupName))));
}

TEST_F(ResourceTreeModelTest, multisensorCameraDisplayNameMapping)
{
    // String constants.
    static constexpr auto kUserDefinedGroupName = "unique_user_defined_group_name";

    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When multisensor camera with certain unique name is added to the server.
    const auto multisensorSubCamera =
        addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // When user defined group name of the multisensor camera is not specified.
    ASSERT_TRUE(multisensorSubCamera->getUserDefinedGroupName().isNull());

    // Then multisensor camera node have group ID as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            iconTypeMatch(QnResourceIconCache::MultisensorCamera),
            displayFullMatch(multisensorSubCamera->getGroupId()))));

    // When user defined group name is set to the multisensor camera.
    multisensorSubCamera->setUserDefinedGroupName(kUserDefinedGroupName);

    // Then multisensor camera node have user defined group name as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            iconTypeMatch(QnResourceIconCache::MultisensorCamera),
            displayFullMatch(multisensorSubCamera->getUserDefinedGroupName()))));
}

TEST_F(ResourceTreeModelTest, cameraNodeIsDragEnabled)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderNodeIsDragEnabled)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When Server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder with certain unique name is added to the server.
    addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, multisensorCameraNodeIsDragEnabled)
{
    // When user is logged in.
    loginAsAdmin("admin");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When multisensor camera with certain unique name is added to the server.
    addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto multisensorCameraIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And that node is draggable.
    ASSERT_TRUE(hasFlag(Qt::ItemIsDragEnabled)(multisensorCameraIndex));
}

} // namespace test
} // namespace nx::vms::client::desktop
