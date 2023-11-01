// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <nx/vms/api/types/access_rights_types.h>

#include "access_subject_editing_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::core::access;
using namespace nx::vms::api;

using ResourceTree::NodeType;

// ------------------------------------------------------------------------------------------------
// Static utility methods tests.

TEST(AccessSubjectEditingContextStaticTest, dependentAccessRights)
{
    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::view),
        AccessRight::viewArchive
        | AccessRight::exportArchive
        | AccessRight::viewBookmarks
        | AccessRight::manageBookmarks
        | AccessRight::userInput
        | AccessRight::edit);

    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::viewArchive),
        AccessRight::exportArchive | AccessRight::viewBookmarks | AccessRight::manageBookmarks);

    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::exportArchive),
        AccessRights{});

    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::viewBookmarks),
        AccessRight::manageBookmarks);

    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::manageBookmarks),
        AccessRights{});

    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::userInput),
        AccessRight::edit);

    EXPECT_EQ(AccessSubjectEditingContext::dependentAccessRights(AccessRight::edit),
        AccessRights{});
}

TEST(AccessSubjectEditingContextStaticTest, requiredAccessRights)
{
    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::view),
        AccessRights{});

    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::viewArchive),
        AccessRight::view);

    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::exportArchive),
        AccessRight::view | AccessRight::viewArchive);

    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::viewBookmarks),
        AccessRight::view | AccessRight::viewArchive);

    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::manageBookmarks),
        AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks);

    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::userInput),
        AccessRight::view);

    EXPECT_EQ(AccessSubjectEditingContext::requiredAccessRights(AccessRight::edit),
        AccessRight::view | AccessRight::userInput);
}

TEST(AccessSubjectEditingContextStaticTest, specialResourceGroup)
{
    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroup(
        NodeType::camerasAndDevices), kAllDevicesGroupId);

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroup(
        NodeType::webPages), kAllWebPagesGroupId);

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroup(
        NodeType::videoWalls), kAllVideoWallsGroupId);

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroup(
        NodeType::layouts), QnUuid{});

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroup(
        NodeType::resource), QnUuid{});
}

TEST(AccessSubjectEditingContextStaticTest, modifyResourceAccessMap)
{
    ResourceAccessMap map;

    // With dependencies.

    AccessSubjectEditingContext::modifyAccessRightMap(map, kAllDevicesGroupId,
        /*modifiedRightsMask*/ AccessRight::viewArchive | AccessRight::edit,
        /*value*/ true,
        /*withDependent*/ true,
        /*relevantRightsMask*/ kFullAccessRights);

    EXPECT_EQ(map.value(kAllDevicesGroupId),
        AccessRight::view
        | AccessRight::viewArchive
        | AccessRight::userInput
        | AccessRight::edit);

    AccessSubjectEditingContext::modifyAccessRightMap(map, kAllDevicesGroupId,
        /*modifiedRightsMask*/ AccessRight::manageBookmarks,
        /*value*/ true,
        /*withDependent*/ true,
        /*relevantRightsMask*/ kFullAccessRights);

    EXPECT_EQ(map.value(kAllDevicesGroupId),
        AccessRight::view
        | AccessRight::viewArchive
        | AccessRight::viewBookmarks
        | AccessRight::manageBookmarks
        | AccessRight::userInput
        | AccessRight::edit);

    AccessSubjectEditingContext::modifyAccessRightMap(map, kAllDevicesGroupId,
        /*modifiedRightsMask*/ AccessRight::viewArchive,
        /*value*/ false,
        /*withDependent*/ true,
        /*relevantRightsMask*/ kFullAccessRights);

    EXPECT_EQ(map.value(kAllDevicesGroupId),
        AccessRight::view | AccessRight::userInput | AccessRight::edit);

    // Without dependencies.

    AccessSubjectEditingContext::modifyAccessRightMap(map, kAllDevicesGroupId,
        /*modifiedRightsMask*/ AccessRight::view,
        /*value*/ false,
        /*withDependent*/ false,
        /*relevantRightsMask*/ kFullAccessRights);

    EXPECT_EQ(map.value(kAllDevicesGroupId),
        AccessRight::userInput | AccessRight::edit);

    // Relevant access rights.

    AccessSubjectEditingContext::modifyAccessRightMap(map, kAllServersGroupId,
        /*modifiedRightsMask*/ kFullAccessRights,
        /*value*/ true,
        /*withDependent*/ true,
        /*relevantRightsMask*/ AccessRight::view);

    EXPECT_EQ(map.value(kAllServersGroupId), AccessRight::view);
}

// ------------------------------------------------------------------------------------------------
// Contextual tests.

class AccessSubjectEditingContextTest: public AccessSubjectEditingFixture
{
};

TEST_F(AccessSubjectEditingContextTest, relevantAccessRights)
{
    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(resource("Camera1")),
        kFullAccessRights);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(resource("Layout1")),
        kFullAccessRights);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(resource("Server1")),
        AccessRight::view);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(resource("WebPage1")),
        AccessRight::view);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(resource("VideoWall1")),
        AccessRight::edit);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(kAllDevicesGroupId),
        kFullAccessRights);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(kAllServersGroupId),
        AccessRight::view);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(kAllWebPagesGroupId),
        AccessRight::view);

    EXPECT_EQ(AccessSubjectEditingContext::relevantAccessRights(kAllVideoWallsGroupId),
        AccessRight::edit);
}

TEST_F(AccessSubjectEditingContextTest, specialResourceGroupFor)
{
    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroupFor(resource("Camera1")),
        kAllDevicesGroupId);

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroupFor(resource("Layout1")), QnUuid{});

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroupFor(resource("Server1")),
        kAllServersGroupId);

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroupFor(resource("WebPage1")),
        kAllWebPagesGroupId);

    EXPECT_EQ(AccessSubjectEditingContext::specialResourceGroupFor(resource("VideoWall1")),
        kAllVideoWallsGroupId);
}

TEST_F(AccessSubjectEditingContextTest, resourceAccessTreeItemInfo)
{
    // Resources.

    auto index = indexOf(resource("Camera1"));
    auto info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::resource);
    EXPECT_EQ(info.target.resource(), resource("Camera1"));
    EXPECT_EQ(info.outerSpecialResourceGroupId, kAllDevicesGroupId);
    EXPECT_EQ(info.relevantAccessRights, kFullAccessRights);

    index = indexOf(resource("Layout1"));
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::resource);
    EXPECT_EQ(info.target.resource(), resource("Layout1"));
    EXPECT_EQ(info.outerSpecialResourceGroupId, QnUuid{});
    EXPECT_EQ(info.relevantAccessRights, kFullAccessRights);

    index = indexOf(resource("WebPage1"));
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::resource);
    EXPECT_EQ(info.target.resource(), resource("WebPage1"));
    EXPECT_EQ(info.outerSpecialResourceGroupId, kAllWebPagesGroupId);
    EXPECT_EQ(info.relevantAccessRights, AccessRight::view);

    index = indexOf(resource("Server1"));
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::resource);
    EXPECT_EQ(info.target.resource(), resource("Server1"));
    EXPECT_EQ(info.outerSpecialResourceGroupId, kAllServersGroupId);
    EXPECT_EQ(info.relevantAccessRights, AccessRight::view);

    index = indexOf(resource("VideoWall1"));
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::resource);
    EXPECT_EQ(info.target.resource(), resource("VideoWall1"));
    EXPECT_EQ(info.outerSpecialResourceGroupId, kAllVideoWallsGroupId);
    EXPECT_EQ(info.relevantAccessRights, AccessRight::edit);

    // Special resource groups.

    index = indexOf(NodeType::camerasAndDevices);
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::specialResourceGroup);
    EXPECT_EQ(info.target.groupId(), kAllDevicesGroupId);
    EXPECT_EQ(info.outerSpecialResourceGroupId, QnUuid{});
    EXPECT_EQ(info.relevantAccessRights, kFullAccessRights);

    index = indexOf(NodeType::webPages);
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::specialResourceGroup);
    EXPECT_EQ(info.target.groupId(), kAllWebPagesGroupId);
    EXPECT_EQ(info.outerSpecialResourceGroupId, QnUuid{});
    EXPECT_EQ(info.relevantAccessRights, AccessRight::view);

    index = indexOf(NodeType::servers);
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::specialResourceGroup);
    EXPECT_EQ(info.target.groupId(), kAllServersGroupId);
    EXPECT_EQ(info.outerSpecialResourceGroupId, QnUuid{});
    EXPECT_EQ(info.relevantAccessRights, AccessRight::view);

    index = indexOf(NodeType::videoWalls);
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::specialResourceGroup);
    EXPECT_EQ(info.target.groupId(), kAllVideoWallsGroupId);
    EXPECT_EQ(info.outerSpecialResourceGroupId, QnUuid{});
    EXPECT_EQ(info.relevantAccessRights, AccessRight::edit);

    // Grouping nodes.

    index = indexOf(NodeType::layouts);
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::groupingNode);
    EXPECT_FALSE(info.target.isValid());
    EXPECT_EQ(info.nodeType, NodeType::layouts);
    EXPECT_EQ(info.outerSpecialResourceGroupId, QnUuid{});
    EXPECT_EQ(info.relevantAccessRights, kFullAccessRights);

    index = indexOf("Group1");
    info = AccessSubjectEditingContext::resourceAccessTreeItemInfo(index);
    EXPECT_EQ(info.type, ResourceAccessTreeItem::Type::groupingNode);
    EXPECT_FALSE(info.target.isValid());
    EXPECT_EQ(info.nodeType, NodeType::customResourceGroup);
    EXPECT_EQ(info.outerSpecialResourceGroupId, kAllDevicesGroupId);
    EXPECT_EQ(info.relevantAccessRights, kFullAccessRights);
}

TEST_F(AccessSubjectEditingContextTest, combinedOwnCheckState)
{
    editingContext->setOwnResourceAccessMap({
        {kAllDevicesGroupId, AccessRight::view},
        {resource("Camera1")->getId(), AccessRight::view | AccessRight::viewArchive},
        {resource("Camera2")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::exportArchive},
        {resource("Camera3")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput},
        {resource("Camera4")->getId(), AccessRight::view},
        {resource("WebPage1")->getId(), AccessRight::view},
        {resource("WebPage2")->getId(), AccessRight::view},
        {kAllServersGroupId, AccessRight::view}});

    // Dummy cases.

    EXPECT_EQ(editingContext->combinedOwnCheckState({}, AccessRight::view), Qt::Unchecked);

    EXPECT_EQ(editingContext->combinedOwnCheckState({QModelIndex()}, AccessRight::view),
        Qt::Unchecked);

    // Resources selection.

    const QModelIndexList camera1_camera3_webpage1({
        indexOf(resource("Camera1")),
        indexOf(resource("Camera3")),
        indexOf(resource("WebPage1"))});

    EXPECT_EQ(
        editingContext->combinedOwnCheckState(camera1_camera3_webpage1, AccessRight::view),
        Qt::Checked);

    EXPECT_EQ(
        editingContext->combinedOwnCheckState(camera1_camera3_webpage1, AccessRight::viewArchive),
        Qt::Checked); //< As viewArchive is not relevant for web pages.

    EXPECT_EQ(
        editingContext->combinedOwnCheckState(camera1_camera3_webpage1, AccessRight::userInput),
        Qt::PartiallyChecked);

    EXPECT_EQ(
        editingContext->combinedOwnCheckState(camera1_camera3_webpage1, AccessRight::edit),
        Qt::Unchecked);

    // Grouping nodes selection.

    EXPECT_EQ(
        editingContext->combinedOwnCheckState({indexOf("Group1")}, AccessRight::view),
        Qt::Checked);

    EXPECT_EQ(
        editingContext->combinedOwnCheckState({indexOf("Group1")}, AccessRight::exportArchive),
        Qt::PartiallyChecked);

    EXPECT_EQ(
        editingContext->combinedOwnCheckState({indexOf("Group1")}, AccessRight::edit),
        Qt::Unchecked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf("Group1"), indexOf(NodeType::layouts)}, AccessRight::view),
        Qt::PartiallyChecked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf("Group1"), indexOf(resource("WebPage1"))}, AccessRight::view),
        Qt::Checked);

    // Special resource groups selection.

    EXPECT_EQ(editingContext->combinedOwnCheckState({indexOf(NodeType::camerasAndDevices)},
        AccessRight::view),
        Qt::Checked);

    EXPECT_EQ(editingContext->combinedOwnCheckState({indexOf(NodeType::camerasAndDevices)},
        AccessRight::viewArchive),
        Qt::Unchecked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::camerasAndDevices), indexOf(NodeType::servers)},
        AccessRight::view),
        Qt::Checked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::camerasAndDevices), indexOf(NodeType::webPages)},
        AccessRight::view),
        Qt::PartiallyChecked); //< kAllWebPagesGroupId is not checked.

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::servers), indexOf("Group1")},
        AccessRight::view),
        Qt::Checked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::webPages), indexOf("Group1")},
        AccessRight::view),
        Qt::PartiallyChecked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::camerasAndDevices), indexOf("Group1")},
        AccessRight::view),
        Qt::Checked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::camerasAndDevices), indexOf("Group1")},
        AccessRight::viewArchive),
        Qt::PartiallyChecked);

    EXPECT_EQ(editingContext->combinedOwnCheckState(
        {indexOf(NodeType::camerasAndDevices), indexOf("Group1")},
        AccessRight::manageBookmarks),
        Qt::Unchecked);
}

TEST_F(AccessSubjectEditingContextTest, modifyAccessRight_dummyCases)
{
    const ResourceAccessMap sourceMap({
        {resource("WebPage1")->getId(), AccessRight::view},
        {kAllServersGroupId, AccessRight::view}});

    editingContext->setOwnResourceAccessMap(sourceMap);

    editingContext->modifyAccessRight(QModelIndex{}, AccessRight::view,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    // Trying to set an irrelevant access right.
    editingContext->modifyAccessRight(indexOf(resource("WebPage2")), AccessRight::viewArchive,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    // Trying to set an access right already set.
    editingContext->modifyAccessRight(indexOf(resource("WebPage1")), AccessRight::view,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    // Trying to set an access right already inherited from a special resource group.
    editingContext->modifyAccessRight(indexOf(resource("Server1")), AccessRight::view,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);
}

TEST_F(AccessSubjectEditingContextTest, modifyAccessRight_toggleResource)
{
    const ResourceAccessMap sourceMap({
        {kAllDevicesGroupId, AccessRight::view | AccessRight::userInput},
        {resource("Camera1")->getId(), AccessRight::view | AccessRight::viewArchive},
        {resource("Camera2")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::exportArchive},
        {resource("Camera3")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput},
        {resource("Layout2")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput}});

    editingContext->setOwnResourceAccessMap(sourceMap);

    editingContext->modifyAccessRight(indexOf(resource("Layout1")), AccessRight::viewBookmarks,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Layout1")->getId()),
        AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks);

    editingContext->modifyAccessRight(indexOf(resource("Layout1")), AccessRight::viewArchive,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Layout1")->getId()),
        AccessRight::view);

    // If we attempt to set AccessRight that is already set in the owning special resource group,
    // the AccessRight will NOT be set.
    editingContext->modifyAccessRight(indexOf(resource("Camera4")), AccessRight::view,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        AccessRights{}); //< Was already set in kAllDevicesGroupId.

    // But if we attempt to set AccessRight that is NOT set in the owning special resource group,
    // and it pulls along a required AccessRight that is set in the owning  special resource group,
    // both AccessRights WILL be set.
    editingContext->modifyAccessRight(indexOf(resource("Camera4")), AccessRight::viewArchive,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        AccessRight::view | AccessRight::viewArchive);

    // Clearing AccessRight that is set in the owning special resource group causes clearing the
    // AccessRight and all depending AccessRights for the resource itself AND the resource group.
    // Other resources in the special resource group GAIN those AccessRights that were cleared!
    const auto accessRightsClearedFromAllDevices = editingContext->ownResourceAccessMap().value(
        kAllDevicesGroupId); //< Because we clear AccessRight::view, all other depend on it.
    editingContext->modifyAccessRight(indexOf(resource("Camera4")), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllDevicesGroupId),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera1")->getId()),
        sourceMap.value(resource("Camera1")->getId()) | accessRightsClearedFromAllDevices);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera2")->getId()),
        sourceMap.value(resource("Camera2")->getId()) | accessRightsClearedFromAllDevices);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        sourceMap.value(resource("Camera3")->getId()) | accessRightsClearedFromAllDevices);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera5")->getId()),
        sourceMap.value(resource("Camera5")->getId()) | accessRightsClearedFromAllDevices);

    // Requirements are set only when they are relevant.
    editingContext->modifyAccessRight(indexOf(resource("VideoWall1")), AccessRight::edit,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("VideoWall1")->getId()),
        AccessRight::edit);

    // Dependencies are cleared only for a relevant base.
    editingContext->modifyAccessRight(indexOf(resource("VideoWall1")), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("VideoWall1")->getId()),
        AccessRight::edit);

    // Without dependent.

    editingContext->modifyAccessRight(indexOf(resource("Camera4")), AccessRight::viewBookmarks,
        /*value*/ true, /*withDependentAccessRights*/ false);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        AccessRight::viewBookmarks);

    editingContext->modifyAccessRight(indexOf(resource("Camera3")), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ false);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        AccessRight::viewArchive | AccessRight::userInput);
}

TEST_F(AccessSubjectEditingContextTest, modifyAccessRight_toggleGroupingNode)
{
    const ResourceAccessMap sourceMap({
        {kAllDevicesGroupId, AccessRight::view | AccessRight::userInput},
        {resource("Camera1")->getId(), AccessRight::view | AccessRight::viewArchive},
        {resource("Camera2")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::exportArchive},
        {resource("Camera3")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput},
        {resource("Layout2")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput}});

    editingContext->setOwnResourceAccessMap(sourceMap);

    // An attempt to set AccessRight that is already set for all child resources does nothing.
    editingContext->modifyAccessRight(indexOf("Group1"), AccessRight::view,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    // An attempt to clear AccessRight that is not set for any child resources does nothing.
    editingContext->modifyAccessRight(indexOf("Group1"), AccessRight::viewBookmarks,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    editingContext->modifyAccessRight(indexOf(NodeType::layouts), AccessRight::viewBookmarks,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Layout1")->getId()),
        AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Layout2")->getId()),
        AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks
        | AccessRight::userInput); //< Had it before.

    editingContext->modifyAccessRight(indexOf(NodeType::layouts), AccessRight::viewArchive,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Layout1")->getId()),
        AccessRight::view);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Layout2")->getId()),
        AccessRight::view | AccessRight::userInput);

    // Clearing AccessRight that is set in the owning special resource group causes clearing the
    // AccessRight and all depending AccessRights for the resource itself AND the resource group.
    // Other resources in the special resource group GAIN those AccessRights that were cleared!
    const auto accessRightsClearedFromAllDevices = editingContext->ownResourceAccessMap().value(
        kAllDevicesGroupId); //< Because we clear AccessRight::view, all other depend on it.
    editingContext->modifyAccessRight(indexOf("Group1"), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera1")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera2")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllDevicesGroupId),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        sourceMap.value(resource("Camera3")->getId()) | accessRightsClearedFromAllDevices);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        sourceMap.value(resource("Camera4")->getId()) | accessRightsClearedFromAllDevices);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera5")->getId()),
        sourceMap.value(resource("Camera5")->getId()) | accessRightsClearedFromAllDevices);

    // Without dependent.

    // Simple setup.
    editingContext->setOwnResourceAccessMap({{resource("Camera1")->getId(), AccessRight::view}});

    editingContext->modifyAccessRight(indexOf("Group1"), AccessRight::viewBookmarks,
        /*value*/ true, /*withDependentAccessRights*/ false);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera1")->getId()),
        AccessRight::view | AccessRight::viewBookmarks);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera2")->getId()),
        AccessRight::viewBookmarks);

    editingContext->modifyAccessRight(indexOf("Group1"), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ false);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera1")->getId()),
        AccessRight::viewBookmarks);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera2")->getId()),
        AccessRight::viewBookmarks);
}

TEST_F(AccessSubjectEditingContextTest, modifyAccessRight_toggleSpecialResourceGroup)
{
    const ResourceAccessMap sourceMap({
        {kAllDevicesGroupId,
            AccessRight::view | AccessRight::viewArchive | AccessRight::exportArchive},
        {resource("Camera1")->getId(), AccessRight::view},
        {resource("Camera2")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks},
        {resource("Camera3")->getId(),
            AccessRight::view | AccessRight::viewArchive | AccessRight::userInput}});

    editingContext->setOwnResourceAccessMap(sourceMap);

    // An attempt to set AccessRight that is already set does nothing to both the resource group
    // and its child resources.
    editingContext->modifyAccessRight(indexOf(NodeType::camerasAndDevices), AccessRight::view,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    // An attempt to clear AccessRight that is not set does nothing to both the resource group
    // and its child resources.
    editingContext->modifyAccessRight(indexOf(NodeType::camerasAndDevices), AccessRight::userInput,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap(), sourceMap);

    // Clearing AccessRight from a special resource group causes clearing the AccessRight and all
    // dependent AccessRights for all child resources of the special resource group.
    editingContext->modifyAccessRight(indexOf(NodeType::camerasAndDevices),
        AccessRight::viewArchive, /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllDevicesGroupId), AccessRight::view);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera1")->getId()),
        AccessRight::view);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera2")->getId()),
        AccessRight::view);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        AccessRight::view | AccessRight::userInput);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera5")->getId()),
        AccessRights{});

    // Setting.

    editingContext->setOwnResourceAccessMap(sourceMap);

    editingContext->modifyAccessRight(indexOf(NodeType::camerasAndDevices),
        AccessRight::viewBookmarks, /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllDevicesGroupId),
        AccessRight::view
        | AccessRight::viewArchive
        | AccessRight::exportArchive
        | AccessRight::viewBookmarks);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera1")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera2")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        AccessRight::view | AccessRight::userInput); //< Kept userInput and its dependency.
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera4")->getId()),
        AccessRights{});
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera5")->getId()),
        AccessRights{});

    // Requirements are set only when they are relevant.
    editingContext->modifyAccessRight(indexOf(NodeType::videoWalls), AccessRight::edit,
        /*value*/ true, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllVideoWallsGroupId),
        AccessRight::edit);

    // Dependencies are cleared only for a relevant base.
    editingContext->modifyAccessRight(indexOf(NodeType::videoWalls), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllVideoWallsGroupId),
        AccessRight::edit);
    editingContext->modifyAccessRight(indexOf(resource("VideoWall1")), AccessRight::view,
        /*value*/ false, /*withDependentAccessRights*/ true);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllVideoWallsGroupId),
        AccessRight::edit);

    // Without dependent.

    editingContext->modifyAccessRight(indexOf(NodeType::camerasAndDevices),
        AccessRight::userInput, /*value*/ true, /*withDependentAccessRights*/ false);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllDevicesGroupId),
        AccessRight::view
        | AccessRight::viewArchive
        | AccessRight::exportArchive
        | AccessRight::viewBookmarks
        | AccessRight::userInput);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        AccessRight::view); //< AccessRight::view stays, dependencies are not tracked.

    editingContext->modifyAccessRight(indexOf(NodeType::camerasAndDevices),
        AccessRight::view, /*value*/ false, /*withDependentAccessRights*/ false);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(kAllDevicesGroupId),
        AccessRight::viewArchive
        | AccessRight::exportArchive
        | AccessRight::viewBookmarks
        | AccessRight::userInput);
    EXPECT_EQ(editingContext->ownResourceAccessMap().value(resource("Camera3")->getId()),
        AccessRights{});
}

} // namespace test
} // namespace nx::vms::client::desktop
