// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_sorting_grouping_test_fixture.h"

#include <QtCore/QCollator>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

// Set of predefined conditions for lookup of interested groups of model indexes. Based mostly on
// icon match, thus the icon mapping are supposed to be correct (relevant tests pass).

static const auto kCompatibleServerCondition =
    allOf(
        iconTypeMatch(QnResourceIconCache::Server),
        anyOf(directChildOf(serversNodeCondition()), topLevelNode()),
        non(iconStatusMatch(QnResourceIconCache::Incompatible)));

static const auto kIncompatibleServerCondition =
    allOf(
        iconTypeMatch(QnResourceIconCache::Server),
        anyOf(directChildOf(serversNodeCondition()), topLevelNode()),
        iconStatusMatch(QnResourceIconCache::Incompatible));

static const auto kWebPageCondition = iconTypeMatch(QnResourceIconCache::WebPage);

static const auto kProxiedWebResourceCondition = iconTypeMatch(QnResourceIconCache::WebPageProxied);

static const auto kLocalVideoCondition =
    allOf(
        directChildOf(localFilesNodeCondition()),
        iconTypeMatch(QnResourceIconCache::Media));

static const auto kLocalImageCondition =
    allOf(
        directChildOf(localFilesNodeCondition()),
        iconTypeMatch(QnResourceIconCache::Image));

static const auto kFileLayoutCondition =
    allOf(
        directChildOf(localFilesNodeCondition()),
        anyOf(
            iconTypeMatch(QnResourceIconCache::ExportedLayout),
            iconTypeMatch(QnResourceIconCache::ExportedEncryptedLayout)));

static const auto kLayoutCondition =
    allOf(
        directChildOf(layoutsNodeCondition()),
        iconTypeMatch(QnResourceIconCache::Layout));

static const auto kSharedLayoutCondition =
    allOf(
        directChildOf(layoutsNodeCondition()),
        iconTypeMatch(QnResourceIconCache::Layout));

static const auto kVideoWallScreenCondition = iconTypeMatch(QnResourceIconCache::VideoWallItem);
static const auto kVideoWallMatrixCondition = iconTypeMatch(QnResourceIconCache::VideoWallMatrix);

const auto kRecorderCondition = iconTypeMatch(QnResourceIconCache::Recorder);
const auto kMulisensorCameraCondition = iconTypeMatch(QnResourceIconCache::MultisensorCamera);
const auto kRecorderOrMulisensorCameraCondition =
    anyOf(
        kRecorderCondition,
        kMulisensorCameraCondition);

const auto kCameraCondition = iconTypeMatch(QnResourceIconCache::Camera);
const auto kIoModuleCondition = iconTypeMatch(QnResourceIconCache::IOModule);
const auto kVirtualCameraCondition = iconTypeMatch(QnResourceIconCache::VirtualCamera);
const auto kCameraOrIoModuleOrVirtualCameraCondition =
    anyOf(
        kCameraCondition,
        kIoModuleCondition,
        kVirtualCameraCondition);

const auto kServerMonitorCondition = iconTypeMatch(QnResourceIconCache::HealthMonitor);

TEST_F(ResourceTreeModelSortingGroupingTest, compatibleServersSorting)
{
    // When user with administrator permissions is logged in.

    // Compatible Servers under "Servers" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kCompatibleServerCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, incompatibleServersSorting)
{
    // When user with administrator permissions is logged in.

    // Incompatible Servers under "Servers" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kIncompatibleServerCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, serversGrouping)
{
    // When user with administrator permissions is logged in.

    // Check that "Servers" children are grouped exactly in the following order:
    // 1. Compatible Servers
    // 2. Incompatible Servers
    ASSERT_TRUE(checkGrouping({kCompatibleServerCondition, kIncompatibleServerCondition}));
}

TEST_F(ResourceTreeModelSortingGroupingTest, webPagesSorting)
{
    // When user with administrator permissions is logged in.

    // Children of "Web Pages" node should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(directChildOf(webPagesNodeCondition())));
}

TEST_F(ResourceTreeModelSortingGroupingTest, showreelsSorting)
{
    // When user with administrator permissions is logged in.

    // Children of "Showreels" node should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(directChildOf(showreelsNodeCondition())));
}

TEST_F(ResourceTreeModelSortingGroupingTest, fileLayoutsSorting)
{
    // When user with administrator permissions is logged in.

    // File Layouts under "Local Resources" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kFileLayoutCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, localVideosSorting)
{
    // When user with administrator permissions is logged in.

    // Local video items under "Local Resources" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kLocalVideoCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, localImagesSorting)
{
    // When user with administrator permissions is logged in.

    // Local image items under "Local Resources" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kLocalImageCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, localResourcesGrouping)
{
    // When user with administrator permissions is logged in.

    // Check that "Local Resources" children are grouped exactly in the following order:
    // 1. File Layouts
    // 2. Videos
    // 3. Images
    ASSERT_TRUE(checkGrouping(
        {kFileLayoutCondition,
        kLocalVideoCondition,
        kLocalImageCondition}));
}

TEST_F(ResourceTreeModelSortingGroupingTest, layoutsSorting)
{
    // When user with administrator permissions is logged in.

    // Non-shared Layouts under "Layouts" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kLayoutCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, sharedLayoutsSorting)
{
    // When user with administrator permissions is logged in.

    // Shared Layouts under "Layouts" node
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(kSharedLayoutCondition));
}

TEST_F(ResourceTreeModelSortingGroupingTest, layoutsGrouping)
{
    // When user with administrator permissions is logged in.

    // Check that "Layouts" children are grouped exactly in the following order:
    // 1. Shared Layouts
    // 2. Layouts
    ASSERT_TRUE(checkGrouping({kSharedLayoutCondition, kLayoutCondition}));
}

TEST_F(ResourceTreeModelSortingGroupingTest, videoWallsSorting)
{
    // When user with administrator permissions is logged in.

    // Video Walls should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(videoWallNodeCondition()));
}

TEST_F(ResourceTreeModelSortingGroupingTest, videoWallScreensSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Video Wall node.
    const auto videoWallIndex =
        firstMatchingIndex(allOf(videoWallNodeCondition(), hasChildren()));

    // Child Video Wall Screens should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(
        allOf(
            directChildOf(videoWallIndex),
            kVideoWallScreenCondition)));
}

TEST_F(ResourceTreeModelSortingGroupingTest, videoWallMatricesSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Video Wall node.
    const auto videoWallIndex =
        firstMatchingIndex(allOf(videoWallNodeCondition(), hasChildren()));

    // Child Video Wall Matrices should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(
        allOf(
            directChildOf(videoWallIndex),
            kVideoWallMatrixCondition)));
}

TEST_F(ResourceTreeModelSortingGroupingTest, videoWallChildrenGrouping)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Video Wall node.
    const auto videoWallIndex =
        firstMatchingIndex(allOf(videoWallNodeCondition(), hasChildren()));

    const auto kVideoWallScreenCondition =
        allOf(
            directChildOf(videoWallIndex),
            iconTypeMatch(QnResourceIconCache::VideoWallItem));

    const auto kVideoWallMatrixCondition =
        allOf(
            directChildOf(videoWallIndex),
            iconTypeMatch(QnResourceIconCache::VideoWallMatrix));

    // Check that Video Wall children are grouped exactly in the following order:
    // 1. Video Wall Screens
    // 2. Video Wall Matrices
    ASSERT_TRUE(checkGrouping({kVideoWallScreenCondition, kVideoWallMatrixCondition}));
}

TEST_F(ResourceTreeModelSortingGroupingTest, serverCamerasIoModulesVirtualCamerasSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    // Get indexes of all Cameras, IO Modules and Virtual Cameras on given Server.
    const auto cameraIoModuleVirtualCameraIndexes = allMatchingIndexes(
        allOf(
            directChildOf(serverIndex),
            kCameraOrIoModuleOrVirtualCameraCondition));

    // Cameras, IO Modules and Virtual Cameras
    // should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(cameraIoModuleVirtualCameraIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, serverRecordersMultisensorCamerasSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    // Get indexes of all Recorders and Multisensor Cameras on given Server.
    const auto recorderMulisensorIndexes = allMatchingIndexes(
        allOf(
            directChildOf(serverIndex),
            kRecorderOrMulisensorCameraCondition));

    // Recorders and Multisensor cameras should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(recorderMulisensorIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, proxiedWebResourcesSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    // Get indexes of all proxied web resources on given Server.
    const auto proxiedWebResourceIndexes = allMatchingIndexes(
        allOf(
            directChildOf(serverIndex),
            kProxiedWebResourceCondition));

    // Proxied web resources should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(proxiedWebResourceIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, serverChildrenGrouping)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    const auto recorderMulisensorCondition =
        allOf(
            directChildOf(serverIndex),
            kRecorderOrMulisensorCameraCondition);

    const auto cameraIoModuleVirtualCameraCondition =
        allOf(
            directChildOf(serverIndex),
            kCameraOrIoModuleOrVirtualCameraCondition);

    const auto proxiedWebResourcesCondition =
        allOf(
            directChildOf(serverIndex),
            kProxiedWebResourceCondition);

    // Check that Server children are grouped exactly in the following order:
    // 1. Recorders and Multisensor Cameras
    // 2. Cameras, IO Modules and Virtual Cameras
    // 3. Proxied web resources.
    ASSERT_TRUE(checkGrouping(
        {recorderMulisensorCondition,
        cameraIoModuleVirtualCameraCondition,
        proxiedWebResourcesCondition}));
}

TEST_F(ResourceTreeModelSortingGroupingTest, layoutItemsSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Layout.
    const auto layoutIndex = firstMatchingIndex(allOf(kLayoutCondition, hasChildren()));

    // Get indexes of all children of given Layout except Server Monitors and Web Pages.
    const auto layoutCamrasAndMediaIndexes = allMatchingIndexes(
        allOf(
            directChildOf(layoutIndex),
            non(kServerMonitorCondition),
            non(kWebPageCondition),
            non(kProxiedWebResourceCondition)));

    // All non Server Monitor, non Web Page items should be sorted alphanumerically and
    // case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(layoutCamrasAndMediaIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, layoutServerMonitorsSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Layout.
    const auto layoutIndex = firstMatchingIndex(allOf(kLayoutCondition, hasChildren()));

    // Get indexes of all Server Monitors on given Layout.
    const auto layoutServerMonitorIndexes = allMatchingIndexes(
        allOf(
            directChildOf(layoutIndex),
            kServerMonitorCondition));

    // Server Monitors should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(layoutServerMonitorIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, layoutWebPagesSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Layout.
    const auto layoutIndex = firstMatchingIndex(allOf(kLayoutCondition, hasChildren()));

    // Get indexes of all Web Pages on given Layout.
    const auto layoutWebPagesIndexes = allMatchingIndexes(
        allOf(
            directChildOf(layoutIndex),
            kWebPageCondition));

    // Server Monitors should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(layoutWebPagesIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, layoutChildrenGrouping)
{
    // When user with administrator permissions is logged in.

    // Get index of some non-empty Layout.
    const auto layoutIndex = firstMatchingIndex(allOf(kLayoutCondition, hasChildren()));

    const auto layoutCamrasAndMediaCondition =
        allOf(
            directChildOf(layoutIndex),
            non(kServerMonitorCondition),
            non(kWebPageCondition),
            non(kProxiedWebResourceCondition));

    const auto layoutServerMonitorCondition =
        allOf(
            directChildOf(layoutIndex),
            kServerMonitorCondition);

    const auto layoutProxiedWebResourceCondition =
        allOf(
            directChildOf(layoutIndex),
            kProxiedWebResourceCondition);

    const auto layoutWebPageCondition =
        allOf(
            directChildOf(layoutIndex),
            kWebPageCondition);

    // Check that Server children are grouped exactly in the following order:
    // 1. Any items except Server Monitors and Web Pages
    // 2. Server Monitors
    // 3. Proxied Web Resources
    // 4. Web Pages
    ASSERT_TRUE(checkGrouping(
        {layoutCamrasAndMediaCondition,
        layoutServerMonitorCondition,
        layoutProxiedWebResourceCondition,
        layoutWebPageCondition}));
}

TEST_F(ResourceTreeModelSortingGroupingTest, multisensorCameraChildrenSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some Multisensor Camera.
    const auto multisensorCameraIndex = firstMatchingIndex(kMulisensorCameraCondition);

    // Get all Multisensor Camera children indexes.
    const auto multisensorSubCamerasIndexes =
        allOf(
            directChildOf(multisensorCameraIndex),
            kCameraCondition);

    // Multisensor camera children should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(multisensorSubCamerasIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, recorderChildrenSorting)
{
    // When user with administrator permissions is logged in.

    // Get index of some Recorder.
    const auto recorderIndex = firstMatchingIndex(kRecorderCondition);

    // Get all Recorder children indexes.
    const auto recorderCameraIndexes = allMatchingIndexes(
        allOf(
            directChildOf(recorderIndex),
            kCameraCondition));

    // Recorder children should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(recorderCameraIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, userResourcesRecordersMultisensorCamerasSorting)
{
    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viever");

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    // Get all Recorders and Multisensor Cameras directly under "Cameras & Resources" node.
    const auto devicesIndexes = allMatchingIndexes(
        allOf(
            directChildOf(serverIndex),
            kRecorderOrMulisensorCameraCondition));

    // Recorders and Multisensor Cameras should be sorted alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(devicesIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, userResourcesCamerasIoModulesVirtualCamerasSorting)
{
    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viever");

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    // Get all Cameras, IO Modules and Virtual Cameras directly under "Cameras & Resources" node.
    const auto devicesIndexes = allMatchingIndexes(
        allOf(
            directChildOf(serverIndex),
            kCameraOrIoModuleOrVirtualCameraCondition));

    // Cameras, IO Modules and Virtual Cameras should be sorted
    // alphanumerically and case-insensitive.
    ASSERT_TRUE(sortingIsAlphanumericCaseInsensitive(devicesIndexes));
}

TEST_F(ResourceTreeModelSortingGroupingTest, userResourcesGrouping)
{
    // When user with live viewer permissions is logged in.
    loginAsLiveViewer("live_viever");

    // Get index of some non-empty Server.
    const auto serverIndex = firstMatchingIndex(allOf(kCompatibleServerCondition, hasChildren()));

    const auto recorderMulisensorCondition =
        allOf(
            directChildOf(serverIndex),
            kRecorderOrMulisensorCameraCondition);

    const auto cameraIoModuleVirtualCameraCondition =
        allOf(
            directChildOf(serverIndex),
            kCameraOrIoModuleOrVirtualCameraCondition);

    // Check that Server children are grouped exactly in the following order:
    // 1. Recorders and Multisensor Cameras
    // 2. Cameras, IO Modules and Virtual Cameras
    ASSERT_TRUE(checkGrouping(
        {recorderMulisensorCondition,
        cameraIoModuleVirtualCameraCondition}));
}
