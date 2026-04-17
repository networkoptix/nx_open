// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/core/test_utils/resource_tree_model_index_condition.h>

#include "resource_tree_model_test_fixture.h"

using namespace nx::vms::api;
using namespace nx::vms::client::core;
using namespace nx::vms::client::core::test::index_condition;
using nx::vms::client::core::ResourceTree::NodeType;

namespace nx::vms::client::mobile {
namespace test {

// String constants.
static constexpr auto kUniqueCameraName = "unique_camera_name";
static constexpr auto kUniqueGroupName = "unique_group_name";
static constexpr auto kUniqueLayoutName = "unique_layout_name";

static constexpr auto kCamerasAndDevicesName = "Cameras & Devices";
static constexpr auto kLayoutsName = "Layouts";
static constexpr auto kAllDevicesName = "All Devices";

// Predefined conditions.
static const auto kUniqueCameraNameCondition = displayFullMatch(kUniqueCameraName);
static const auto kUniqueGroupNameCondition = displayFullMatch(kUniqueGroupName);
static const auto kUniqueLayoutNameCondition = displayFullMatch(kUniqueLayoutName);

static const auto kCamerasAndDevicesNameCondition = displayFullMatch(kCamerasAndDevicesName);
static const auto kLayoutsNameCondition = displayFullMatch(kLayoutsName);
static const auto kAllDevicesNameCondition = displayFullMatch(kAllDevicesName);

static const auto kCamerasAndDevicesNodeCondition = allOf(
    topLevelNode(),
    kCamerasAndDevicesNameCondition);

static const auto kLayoutsNodeCondition = allOf(
    topLevelNode(),
    kLayoutsNameCondition);

static const auto kAllDevicesNodeCondition = allOf(
    directChildOf(kLayoutsNodeCondition),
    kAllDevicesNameCondition);

TEST_F(ResourceTreeModelTest, emptyTreeHasTwoTopLevelNodes)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then there are two rows in the resource tree.
    ASSERT_EQ(model()->rowCount(), 2);

    // Which are "Cameras & Devices" and "Layouts" groups.
    ASSERT_TRUE(onlyOneMatches(kCamerasAndDevicesNodeCondition));
    ASSERT_TRUE(onlyOneMatches(kLayoutsNodeCondition));
}

TEST_F(ResourceTreeModelTest, topLevelNodesOrder)
{
    // When user is logged in.
    loginAsAdministrator("administrator");

    // Expected top-level nodes order.
    std::vector<QString> referenceOrder = {
        kLayoutsName,
        kCamerasAndDevicesName};

    // Check tree.
    ASSERT_EQ(
        transformToDisplayStrings(allMatchingIndexes(topLevelNode())),
        referenceOrder);
}

TEST_F(ResourceTreeModelTest, camerasAndDevicesNodeExcludedFromSearch)
{
    // When user is logged in.
    loginAsAdministrator("administrator");

    // When search query that matches "Cameras & Devices" top level node display text is entered.
    setSearchString(kCamerasAndDevicesName);

    // Then nothing displayed in the resource tree since there are no matching resources.
    ASSERT_EQ(model()->rowCount(), 0);
}

TEST_F(ResourceTreeModelTest, layoutsNodeExcludedFromSearch)
{
    // When user is logged in.
    loginAsAdministrator("administrator");

    // When search query that matches "Layouts" top level node display text is entered.
    setSearchString(kLayoutsName);

    // Then nothing displayed in the resource tree since there are no matching resources.
    ASSERT_EQ(model()->rowCount(), 0);
}

TEST_F(ResourceTreeModelTest, allDevicesNodeIsChildOfLayoutsNode)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then there is "All Devices" node in the Resource tree.
    auto allDevicesIndex = uniqueMatchingIndex(kAllDevicesNameCondition);

    // Which is child of "Layouts" group.
    ASSERT_TRUE(directChildOf(kLayoutsNodeCondition)(allDevicesIndex));
}

TEST_F(ResourceTreeModelTest, allDevicesNodeIcon)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then there is "All Devices" node in the Resource tree.
    auto allDevicesIndex = uniqueMatchingIndex(kAllDevicesNodeCondition);

    // Which has cameras icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::Cameras)(allDevicesIndex));
}

TEST_F(ResourceTreeModelTest, allDevicesNodeData)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // Then there is "All Devices" node in the Resource tree.
    auto allDevicesIndex = uniqueMatchingIndex(kAllDevicesNodeCondition);

    // Which has correspondent node type.
    ASSERT_TRUE(nodeTypeDataMatch(NodeType::allDevices)(allDevicesIndex));
}

TEST_F(ResourceTreeModelTest, allDevicesNodeIsPinnedToTop)
{
    // When user is logged in.
    auto user = loginAsPowerUser("power_user");

    // Given two layouts on the server.
    const auto server = addServer("server");
    addLayout("z_layout", user->getId());
    addLayout("0_layout", user->getId());

    // Expected order of "Layouts" children.
    std::vector<QString> referenceOrder = {
        kAllDevicesName,
        "0_layout",
        "z_layout"};

    ASSERT_EQ(
        transformToDisplayStrings(allMatchingIndexes(directChildOf(kLayoutsNodeCondition))),
        referenceOrder);
}

TEST_F(ResourceTreeModelTest, allDevicesNodeIsSearchable)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When layout resource with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // When search query that matches this layout is entered.
    setSearchString(kUniqueLayoutName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueLayoutNameCondition));

    // Then there is no "All Devices" item in the resource three.
    ASSERT_TRUE(noneMatches(kAllDevicesNameCondition));

    // When search query that matches "All Devices" node is entered.
    setSearchString(kAllDevicesName);

    // Then "All Devices" is shown in the resource tree as usual.
    ASSERT_TRUE(onlyOneMatches(kAllDevicesNameCondition));
    ASSERT_TRUE(onlyOneMatches(kAllDevicesNodeCondition));
}

TEST_F(ResourceTreeModelTest, customGroupHasNoChildrenIfTheyDontMatchSearch)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When camera within a group is added to the server.
    const auto server = addServer("server");
    const auto camera = addCamera("camera", server->getId());
    core::entity_resource_tree::resource_grouping::setResourceCustomGroupId(camera, "group");

    // When search string that matches group name but not camera name applied.
    setSearchString("group");

    // Then matching group is displayed in the resource tree with no children.
    ASSERT_TRUE(onlyOneMatches(allOf(
        displayFullMatch("group"),
        hasNoChildren())));
    ASSERT_TRUE(noneMatches(displayFullMatch("camera")));
}

TEST_F(ResourceTreeModelTest, recorderGroupHasNoChildrenIfTheyDontMatchSearch)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When multisensor camera added to the server.
    const auto server = addServer("server");
    const auto camera = addRecorderCamera("camera", "recorder", server->getId());
    camera->setDefaultGroupName("recorder");

    // When search string that matches multisensor group name but not camera name applied.
    setSearchString("recorder");

    // Then matching multisensor group displayed in the resource tree with no children.
    ASSERT_TRUE(onlyOneMatches(allOf(
        displayFullMatch("recorder"),
        hasNoChildren())));
    ASSERT_TRUE(noneMatches(displayFullMatch("camera")));
}

TEST_F(ResourceTreeModelTest, searchStringMatchesNoneOfCameras)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Given two cameras on the server.
    const auto server = addServer("server");
    addCamera("camera_1", server->getId());
    addCamera("camera_2", server->getId());

    // When search string that matches none of camera names applied.
    setSearchString("camera_3");

    // Then no camera items appear in the resource tree.
    ASSERT_TRUE(noneMatches(cameraIconTypeCondition()));
}

TEST_F(ResourceTreeModelTest, searchStringMatchesCamera)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Given two cameras on the server.
    const auto server = addServer("server");
    addCamera("camera_1", server->getId());
    addCamera("camera_2", server->getId());

    // When search string that matches one of camera names applied.
    setSearchString("camera_1");

    // Then camera item appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch("camera_1")));
}

TEST_F(ResourceTreeModelTest, cameraAdds)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

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

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueCameraNameCondition));

    // When previously added camera resource is removed from resource pool.
    resourcePool()->removeResource(camera);

    // Then no more nodes with corresponding display text found in the resource tree.
    ASSERT_TRUE(noneMatches(kUniqueCameraNameCondition));
}

TEST_F(ResourceTreeModelTest, cameraRenames)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server and then renamed.
    const auto camera = addCamera("camera", server->getId());
    camera->setName("renamed_camera");

    // Then no nodes with original camera name appears in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch("camera")));

    // Then single node with new camera name appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch("renamed_camera")));
}

TEST_F(ResourceTreeModelTest, cameraIconType)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera resource with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it have camera icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::Camera)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, ioModuleIconType)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When IO Module with certain unique name is added to the server.
    addIOModule(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it have IO module icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::IOModule)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, multisensorCameraIconType)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When multisensor camera with certain unique name is added to the server.
    addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto multisensorCameraIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it have multisensor camera icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::MultisensorCamera)(multisensorCameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderIconType)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder with certain unique name is added to the server.
    addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it have recorder icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::Recorder)(recorderIndex));
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

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // Then exactly one node with user group name display text appears in the resource tree and
    // there are no other group names appear in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUserGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kDefaultGroupName)));

    logout();

    // Order of cameras doesn't matter.
    cameraList.first()->setUserDefinedGroupName({});
    cameraList.last()->setUserDefinedGroupName(kUserGroupName);

    loginAsPowerUser("power_user");
    ASSERT_TRUE(onlyOneMatches(displayFullMatch(kUserGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kUniqueGroupName)));
    ASSERT_TRUE(noneMatches(displayFullMatch(kDefaultGroupName)));
}

TEST_F(ResourceTreeModelTest, virtualCameraIconType)
{
    // When User with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When virtual camera with certain unique name is added to the server.
    addVirtualCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto virtualCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it have virtual camera icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::VirtualCamera)(virtualCameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIconStatus)
{
    using namespace nx::vms::api;

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When Offline status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::offline);

    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // Then camera icon has Offline decoration.
    ASSERT_TRUE(iconStatusMatch(ResourceIconCache::Offline)(cameraIndex));

    // When Unauthorized status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::unauthorized);

    // Then camera icon has Unauthorized decoration.
    ASSERT_TRUE(iconStatusMatch(ResourceIconCache::Unauthorized)(cameraIndex));

    // When Incompatible status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::incompatible);

    // Then camera icon has Incompatible decoration.
    ASSERT_TRUE(iconStatusMatch(ResourceIconCache::Incompatible)(cameraIndex));

    // When Offline status is set to the camera resource.
    camera->setStatus(nx::vms::api::ResourceStatus::offline);
    // And issue occurred on the camera.
    camera->addStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag);

    // Than camera icon has Offline decoration.
    ASSERT_TRUE(iconStatusMatch(ResourceIconCache::Offline)(cameraIndex));

    // When status is changed to Online.
    camera->setStatus(nx::vms::api::ResourceStatus::online);
    // Than camera icon has Incompatible decoration
    ASSERT_TRUE(iconStatusMatch(ResourceIconCache::Incompatible)(cameraIndex));

    // When there is no more issue on the camera.
    camera->removeStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag);
    // Than camera icon has Online decoration
    ASSERT_TRUE(iconStatusMatch(ResourceIconCache::Online)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraScheduledExtraStatus)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // When setLicenseUsed flag set true for the camera resource.
    camera->setScheduleEnabled(true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it provides scheduled extra status flag.
    ASSERT_TRUE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::scheduled)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraBuggyExtraStatus)
{
    using namespace nx::vms::api;

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera resource with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // It has no buggy status by default.
    ASSERT_FALSE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::buggy)(cameraIndex));

    // When issue occurred on the camera.
    camera->addStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag);

    // It provides buggy extra status flag.
    ASSERT_TRUE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::buggy)(cameraIndex));

    // When schedule is marked as invalid.
    camera->addStatusFlags(CameraStatusFlag::CSF_InvalidScheduleFlag);

    // Then camera is still buggy.
    ASSERT_TRUE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::buggy)(cameraIndex));

    // Even when camera issues stop.
    camera->removeStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag);

    // Then camera is still buggy.
    ASSERT_TRUE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::buggy)(cameraIndex));

    // And when camera schedule is back to normal.
    camera->removeStatusFlags(CameraStatusFlag::CSF_InvalidScheduleFlag);

    // Then camera is not buggy anymore.
    ASSERT_FALSE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::buggy)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsNonEditableByPowerUser)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one camera node with corresponding display text appears in the tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsNonEditableByCustomUserWithEditCamerasPermissions)
{
    // When custom user with is logged in.
    const auto user = loginAsCustomUser("custom_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // And custom user gets access to that camera.
    setupAccessToResourceForUser(user, camera, AccessRight::view | AccessRight::edit);

    // Then exactly one camera node with corresponding display text appears in the tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraIsNotEditableByPlainCustomUser)
{
    // When custom user with permission to edit camera settings is logged in.
    const auto user = loginAsCustomUser("custom_with_edit_cameras");

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

TEST_F(ResourceTreeModelTest, recorderGroupIsNotEditableByAdmin)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    addRecorderCamera("recorder", kUniqueGroupName, server->getId());

    // Then exactly one recorder group node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderGroupIsNotEditableByCustomUserWithEditCamerasPermissions)
{
    // When custom user is logged in.
    const auto user = loginAsCustomUser("custom_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with unique group name is added to the server.
    const auto recorderCamera = addRecorderCamera("recorder", kUniqueGroupName, server->getId());

    // And custom user gets access to that recorder camera.
    setupAccessToResourceForUser(user, recorderCamera, AccessRight::view | AccessRight::edit);

    // Then exactly one recorder group node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, recorderGroupIsNotEditableByPlainCustomUser)
{
    // When custom user is logged in.
    const auto user = loginAsCustomUser("custom_user");

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

TEST_F(ResourceTreeModelTest, singleCameraRecorderGroupExtraStatus)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder camera with certain unique name is added to the server.
    const auto camera = addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one recorder camera node with corresponding display text appears in the tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And it provides empty extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(ResourceExtraStatusFlag::empty)(recorderIndex));

    // When setLicenseUsed flag set true for the camera resource.
    camera->setScheduleEnabled(true);

    // Then recorder node provides scheduled extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(ResourceExtraStatusFlag::scheduled)(recorderIndex));

    // When status changed to Recording.
    camera->setStatus(nx::vms::api::ResourceStatus::recording);

    // Then recorder node provides recording extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch({ResourceExtraStatusFlag::scheduled,
        ResourceExtraStatusFlag::recording})(recorderIndex));
}

TEST_F(ResourceTreeModelTest, twoCamerasRecorderGroupExtraStatus)
{
    // String constants
    static constexpr auto kUniqueCameraName1 = "unique_camera_name_1";
    static constexpr auto kUniqueCameraName2 = "unique_camera_name_2";

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When two recorder cameras with different unique names is added to the server.
    const auto camera1 = addRecorderCamera(kUniqueCameraName1, kUniqueGroupName, server->getId());
    const auto camera2 = addRecorderCamera(kUniqueCameraName2, kUniqueGroupName, server->getId());

    // Then there are two recorder cameras with corresponding names in the resource tree.
    const auto camera1Index = uniqueMatchingIndex(displayFullMatch(kUniqueCameraName1));
    const auto camera2Index = uniqueMatchingIndex(displayFullMatch(kUniqueCameraName2));

    // When setLicenseUsed flag set true for the second recorder camera.
    camera2->setScheduleEnabled(true);

    // Then second recorder camera provides scheduled extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(ResourceExtraStatusFlag::scheduled)(camera2Index));

    // As well as parent recorder group node.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(ResourceExtraStatusFlag::scheduled)(
        camera2Index.parent()));

    // First recorder camera provides no extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(ResourceExtraStatusFlag::empty)(camera1Index));

    // When status of the second recorder camera changed to Recording.
    camera2->setStatus(nx::vms::api::ResourceStatus::recording);

    // Then second recorder camera also provides recording extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(
        {ResourceExtraStatusFlag::scheduled, ResourceExtraStatusFlag::recording})(
        camera2Index));

    // As well as parent recorder group node.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(
        {ResourceExtraStatusFlag::scheduled, ResourceExtraStatusFlag::recording})(
        camera2Index.parent()));

    // First recorder camera still provides no extra status flag.
    ASSERT_TRUE(resourceExtraStatusFlagsMatch(ResourceExtraStatusFlag::empty)(camera1Index));
}

TEST_F(ResourceTreeModelTest, cameraIsDisplayedForOnHiddenEdgeServer)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When edge server is added to the resource pool.
    const auto server = addEdgeServer("server", "192.168.0.0");

    // When edge camera with certain unique name is added to the server.
    const auto camera = addEdgeCamera(kUniqueCameraName, server);

    // When redundancy flag set to false.
    server->setRedundancy(false);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node have camera icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::Camera)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderCameraIsChildOfRecorderNode)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

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

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

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

    // And that node is the child of the second recorder node.
    ASSERT_TRUE(directChildOf(displayFullMatch(kUniqueGroupName2))(cameraIndex));
}

TEST_F(ResourceTreeModelTest, singleRecorderCameraMovesToAnotherRecorder)
{
    // String constants
    static constexpr auto kUniqueGroupName1 = "unique_group_name_1";
    static constexpr auto kUniqueGroupName2 = "unique_group_name_2";

    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

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
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

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
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When multisensor sub-camera with certain unique name is added to the server.
    addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto multisensorSubCameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node is the child of multisensor camera node.
    ASSERT_TRUE(directChildOf(kUniqueGroupNameCondition)(multisensorSubCameraIndex));
}

TEST_F(ResourceTreeModelTest, cameraNodeProvidesResource)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    const auto camera = addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node provides pointer to the camera resource.
    ASSERT_TRUE(dataMatch(core::ResourceRole, QVariant::fromValue<QnResourcePtr>(camera))(
        cameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderDisplayNameMapping)
{
    // String constants.
    static constexpr auto kUserDefinedGroupName = "unique_user_defined_group_name";

    // When user is logged in.
    loginAsPowerUser("power_user");

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
            iconTypeMatch(ResourceIconCache::Recorder),
            displayFullMatch(recorderCamera->getGroupId()))));

    // When user defined group name is set to the recorder camera.
    recorderCamera->setUserDefinedGroupName(kUserDefinedGroupName);

    // Then recorder node have user defined group name as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            iconTypeMatch(ResourceIconCache::Recorder),
            displayFullMatch(kUserDefinedGroupName))));
}

TEST_F(ResourceTreeModelTest, recorderGroupRenamedWhenGroupNameSetToTheLastAddedCamera)
{
    // String constants.
    static constexpr auto kUserDefinedGroupName = "unique_user_defined_group_name";

    // When user is logged in.
    loginAsPowerUser("power_user");

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
        iconTypeMatch(ResourceIconCache::Recorder),
        displayFullMatch(kUserDefinedGroupName))));
}

TEST_F(ResourceTreeModelTest, multisensorCameraDisplayNameMapping)
{
    // String constants.
    static constexpr auto kUserDefinedGroupName = "unique_user_defined_group_name";

    // When user is logged in.
    loginAsPowerUser("power_user");

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
            iconTypeMatch(ResourceIconCache::MultisensorCamera),
            displayFullMatch(multisensorSubCamera->getGroupId()))));

    // When user defined group name is set to the multisensor camera.
    multisensorSubCamera->setUserDefinedGroupName(kUserDefinedGroupName);

    // Then multisensor camera node have user defined group name as display text.
    ASSERT_TRUE(onlyOneMatches(
        allOf(
            iconTypeMatch(ResourceIconCache::MultisensorCamera),
            displayFullMatch(multisensorSubCamera->getUserDefinedGroupName()))));
}

TEST_F(ResourceTreeModelTest, cameraNodeIsNotDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When camera with certain unique name is added to the server.
    addCamera(kUniqueCameraName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto cameraIndex = uniqueMatchingIndex(kUniqueCameraNameCondition);

    // And that node isn't draggable.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDragEnabled)(cameraIndex));
}

TEST_F(ResourceTreeModelTest, recorderNodeIsNotDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When Server is added to the resource pool.
    const auto server = addServer("server");

    // When recorder with certain unique name is added to the server.
    addRecorderCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto recorderIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And that node isn't draggable.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDragEnabled)(recorderIndex));
}

TEST_F(ResourceTreeModelTest, multisensorCameraNodeIsNotDragEnabled)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When server is added to the resource pool.
    const auto server = addServer("server");

    // When multisensor camera with certain unique name is added to the server.
    addMultisensorSubCamera(kUniqueCameraName, kUniqueGroupName, server->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto multisensorCameraIndex = uniqueMatchingIndex(kUniqueGroupNameCondition);

    // And that node isn't draggable.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDragEnabled)(multisensorCameraIndex));
}

TEST_F(ResourceTreeModelTest, layoutAdds)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When layout resource with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, layoutIsChildOfLayoutsNode)
{
    // When user with power user permissions is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When layout resource with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then there is a single node that matches camera name found in the resource tree.
    auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it's child of "Layouts" node.
    ASSERT_TRUE(directChildOf(kLayoutsNodeCondition)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutsAreGroupedByType)
{
    // When user with power user permissions is logged in.
    const auto user = loginAsPowerUser("power_user");

    // Given shared and owned layouts.
    addLayout("z_layout", user->getId());
    addLayout("z_shared_layout", nx::Uuid());
    addLayout("0_layout", user->getId());
    addLayout("0_shared_layout", nx::Uuid());

    // Expected order of layouts.
    std::vector<QString> referenceOrder = {
        kAllDevicesName,
        "0_shared_layout",
        "z_shared_layout",
        "0_layout",
        "z_layout"};

    // Check tree.
    ASSERT_EQ(
        transformToDisplayStrings(allMatchingIndexes(directChildOf(kLayoutsNodeCondition))),
        referenceOrder);
}

TEST_F(ResourceTreeModelTest, layoutRemoves)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(kUniqueLayoutNameCondition));

    // When previously added layout is removed from resource pool.
    resourcePool()->removeResource(layout);

    // Then no more nodes with corresponding display text found in the Resource Tree.
    ASSERT_TRUE(noneMatches(kUniqueLayoutNameCondition));
}

TEST_F(ResourceTreeModelTest, layoutRenames)
{
    // When user with power user permissions is logged in.
    auto user = loginAsPowerUser("power_user");

    // When camera with certain unique name is added to the server and then renamed.
    const auto layout = addLayout("layout", user->getId());
    layout->setName("renamed_layout");

    // Then no nodes with original layout name appears in the resource tree.
    ASSERT_TRUE(noneMatches(displayFullMatch("layout")));

    // Then single node with new layout name appears in the resource tree.
    ASSERT_TRUE(onlyOneMatches(displayFullMatch("renamed_layout")));
}

TEST_F(ResourceTreeModelTest, layoutIconType)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node have layout icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::Layout)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIconType)
{
    // When user is logged in.
    loginAsPowerUser("power_user");

    // When non-owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node have shared layout icon type.
    ASSERT_TRUE(iconTypeMatch(ResourceIconCache::SharedLayout)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, lockedLayoutIconType)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    const auto regularLayoutIconCondition = allOf(
        iconStatusMatch(ResourceIconCache::Unknown), iconTypeMatch(ResourceIconCache::Layout));

    // After layout locking...
    layout->setLocked(true);

    // That node have locked layout extra status icon.
    ASSERT_TRUE(regularLayoutIconCondition(layoutIndex));
    ASSERT_TRUE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::locked)(layoutIndex));

    // After unlocking it back...
    layout->setLocked(false);

    // That node displays plain layout icon again.
    ASSERT_TRUE(regularLayoutIconCondition(layoutIndex));
    ASSERT_FALSE(hasResourceExtraStatusFlag(ResourceExtraStatusFlag::locked)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, ownLayoutIsNotEditableByAdmin)
{
    // When user with power user permissions is logged in.
    const auto powerUser = loginAsPowerUser("power_user");

    // When layout with unique name owned by user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, powerUser->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIsNotEditableByAdmin)
{
    // When user with power user permissions is logged in.
    loginAsPowerUser("power_user");

    // When shared layout with unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, nx::Uuid());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, sharedLayoutIsNotEditableByNonAdmin)
{
    // When user without power user permissions is logged in.
    const auto liveViewerUser = loginAsLiveViewer("live_viever");

    // When shared layout with unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, nx::Uuid());

    // And access to the shared layout is granted to the logged in user.
    setupAccessToResourceForUser(liveViewerUser, layout, true);

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, ownLayoutIsNotEditableByNonAdmin)
{
    // When user without power user permissions is logged in.
    const auto liveViewerUser = loginAsLiveViewer("live_viever");

    // When layout with unique name owned by mentioned user is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, liveViewerUser->getId());

    // Then exactly one node with corresponding display texts appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And it doesn't have editable flag.
    ASSERT_FALSE(hasFlag(Qt::ItemIsEditable)(layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutProvidesResource)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node provides pointer to the layout resource.
    ASSERT_TRUE(dataMatch(core::ResourceRole, QVariant::fromValue<QnResourcePtr>(layout))(
        layoutIndex));
}

TEST_F(ResourceTreeModelTest, layoutNodeIsNotDragEnabled)
{
    // When user is logged in.
    const auto user = loginAsPowerUser("power_user");

    // When owned layout with certain unique name is added to the resource pool.
    const auto layout = addLayout(kUniqueLayoutName, user->getId());

    // Then exactly one node with corresponding display text appears in the resource tree.
    const auto layoutIndex = uniqueMatchingIndex(kUniqueLayoutNameCondition);

    // And that node isn't draggable.
    ASSERT_FALSE(hasFlag(Qt::ItemIsDragEnabled)(layoutIndex));
}

} // namespace test
} // namespace nx::vms::client::mobile
