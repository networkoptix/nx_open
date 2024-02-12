// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_sorting_grouping_test_fixture.h"

#include <QtCore/QCollator>

#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace index_condition;

void ResourceTreeModelSortingGroupingTest::SetUp()
{
    base_class::SetUp();
    const auto user = loginAsPowerUser("power_user");
    const auto userId = user->getId();
    createAllKindsOfResources(userId);
}

QStringList ResourceTreeModelSortingGroupingTest::sortingSignificantStrings() const
{
    return {"10_1", "2_11", "1_2", "b", "A", "C"};
}

bool ResourceTreeModelSortingGroupingTest::sortingIsAlphanumericCaseInsensitive(
    Condition condition) const
{
    return sortingIsAlphanumericCaseInsensitive(allMatchingIndexes(condition));
}

bool ResourceTreeModelSortingGroupingTest::sortingIsAlphanumericCaseInsensitive(
    const QModelIndexList& indexes) const
{
    NX_ASSERT(indexes.size() >= sortingSignificantStrings().size());

    const auto displayedSequence = transformToDisplayStrings(indexes);

    auto properlySortedSequence = displayedSequence;
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(properlySortedSequence.begin(), properlySortedSequence.end(),
        [collator](const QString& l, const QString& r) { return collator.compare(l, r) < 0; });

    return displayedSequence == properlySortedSequence;
}

bool ResourceTreeModelSortingGroupingTest::checkGrouping(
    const std::vector<Condition>& conditionsInOrder) const
{
    const auto rowLessThan =
        [](const QModelIndex& l, const QModelIndex& r) { return l.row() < r.row(); };

    const auto indexesAreContinuous =
        [](const QModelIndexList& indexes) -> bool
        {
            bool result = true;
            for (int i = 0; i < indexes.size() - 1; ++i)
                result = result && (indexes.at(i).row() == indexes.at(i + 1).row() - 1);
            return result;
        };

    const auto indexesHaveSameParent =
        [](const QModelIndexList& indexes) -> bool
        {
            bool result = true;
            for (int i = 0; i < indexes.size() - 1; ++i)
                result = result && (indexes.at(i).parent() == indexes.at(i + 1).parent());
            return result;
        };

    std::vector<int> lowerRows;
    std::vector<int> upperRows;

    for (const auto& condition: conditionsInOrder)
    {
        auto matchingIndexes = allMatchingIndexes(condition);
        NX_ASSERT(!matchingIndexes.isEmpty());
        NX_ASSERT(indexesAreContinuous(matchingIndexes));
        NX_ASSERT(indexesHaveSameParent(matchingIndexes));
        if (!matchingIndexes.isEmpty())
            lowerRows.push_back(matchingIndexes.first().row());
        if (!matchingIndexes.isEmpty())
            upperRows.push_back(matchingIndexes.last().row());
    }
    return std::is_sorted(lowerRows.begin(), lowerRows.end())
        && std::is_sorted(upperRows.begin(), upperRows.end());
}

void ResourceTreeModelSortingGroupingTest::createAllKindsOfResources(
    const nx::Uuid& parentUserId) const
{
    createSharedLayouts();
    createFileLayouts();
    createVideoWallsWithScreensAndMatrices();
    const auto webPages = createWebPages();
    const auto integrations = createIntegrations();
    const auto localImages = createLocalImages();
    const auto localVideos = createLocalVideos();

    createShowreels(parentUserId);
    const auto layouts = createLayouts(parentUserId);
    const auto firstLayout = layouts.first();

    const auto incompatibleServers = createIncompatibleServers();
    const auto compatibleServers = createCompatibleServers();
    const auto firstCompatibleServer = compatibleServers.first();

    createRecorders(firstCompatibleServer->getId());
    createMultisensorCameras(firstCompatibleServer->getId());
    const auto cameras = createCameras(firstCompatibleServer->getId());
    const auto ioModules = createIoModules(firstCompatibleServer->getId());
    const auto virtualCameras = createVirtualCameras(firstCompatibleServer->getId());
    const auto proxiedWebResources = createProxiedWebResources(firstCompatibleServer->getId());
    const auto proxiedIntegrations = createProxiedIntegrations(firstCompatibleServer->getId());

    addToLayout(firstLayout, compatibleServers + incompatibleServers + localImages + webPages
        + integrations + localVideos + cameras + virtualCameras + ioModules + proxiedWebResources
        + proxiedIntegrations);
}

void ResourceTreeModelSortingGroupingTest::createShowreels(const nx::Uuid& parentUserId) const
{
    for (const auto showreelName: sortingSignificantStrings())
        addShowreel(showreelName, parentUserId);
}

void ResourceTreeModelSortingGroupingTest::createVideoWallsWithScreensAndMatrices() const
{
    for (const auto videoWallName: sortingSignificantStrings())
    {
        const auto videoWall = addVideoWall(videoWallName);
        for (const auto videoWallChildName: sortingSignificantStrings())
        {
            addVideoWallScreen(videoWallChildName, videoWall);
            addVideoWallMatrix(videoWallChildName, videoWall);
        }
    }
}

void ResourceTreeModelSortingGroupingTest::createFileLayouts() const
{
    for (const auto fileLayoutName: sortingSignificantStrings())
        addFileLayout(fileLayoutName);
}

void ResourceTreeModelSortingGroupingTest::createSharedLayouts() const
{
    for (const auto sharedLayoutName: sortingSignificantStrings())
        addLayout(sharedLayoutName);
}

void ResourceTreeModelSortingGroupingTest::createRecorders(const nx::Uuid& parentServerId) const
{
    for (const auto recorderName: sortingSignificantStrings())
    {
        for (const auto recorderSubCameraName: sortingSignificantStrings())
            addRecorderCamera(recorderSubCameraName, "recorder_" + recorderName, parentServerId);
    }
}

void ResourceTreeModelSortingGroupingTest::createMultisensorCameras(
    const nx::Uuid& parentServerId) const
{
    for (const auto multisensorCameraName: sortingSignificantStrings())
    {
        for (const auto multisensorSubCameraName: sortingSignificantStrings())
        {
            addMultisensorSubCamera(
                multisensorSubCameraName,
                "multisensor_camera_" + multisensorCameraName,
                parentServerId);
        }
    }
}

QnResourceList ResourceTreeModelSortingGroupingTest::createIncompatibleServers() const
{
    QnResourceList result;
    for (const auto incompatibleServerName: sortingSignificantStrings())
    {
        const auto incompatibleServer = addServer(incompatibleServerName);
        incompatibleServer->setStatus(nx::vms::api::ResourceStatus::incompatible);
        result.append(incompatibleServer);
    }
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createCompatibleServers() const
{
    QnResourceList result;
    for (const auto compatibleServerName: sortingSignificantStrings())
        result.append(addServer(compatibleServerName));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createWebPages() const
{
    QnResourceList result;
    for (const auto webPageName: sortingSignificantStrings())
        result.append(addWebPage(webPageName, WebPageSubtype::none));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createIntegrations() const
{
    QnResourceList result;
    for (const auto integrationName: sortingSignificantStrings())
        result.append(addWebPage(integrationName, WebPageSubtype::clientApi));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createProxiedWebResources(
    const nx::Uuid& parentServerId) const
{
    QnResourceList result;
    for (const auto proxiedWebResourceName: sortingSignificantStrings())
    {
        result.append(
            addProxiedWebPage(proxiedWebResourceName, WebPageSubtype::none, parentServerId));
    }
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createProxiedIntegrations(
    const nx::Uuid& parentServerId) const
{
    QnResourceList result;
    for (const auto proxiedIntegrationName: sortingSignificantStrings())
    {
        result.append(
            addProxiedWebPage(proxiedIntegrationName, WebPageSubtype::clientApi, parentServerId));
    }
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createLocalImages() const
{
    QnResourceList result;
    for (const auto localImageNameBase: sortingSignificantStrings())
        result.append(addLocalMedia(localImageNameBase + ".png"));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createLocalVideos() const
{
    QnResourceList result;
    for (const auto localVideoNameBase: sortingSignificantStrings())
        result.append(addLocalMedia(localVideoNameBase + ".mkv"));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createCameras(
    const nx::Uuid& parentServerId) const
{
    QnResourceList result;
    for (const auto cameraName: sortingSignificantStrings())
        result.append(addCamera(cameraName, parentServerId));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createIoModules(
    const nx::Uuid& parentServerId) const
{
    QnResourceList result;
    for (const auto ioModuleName: sortingSignificantStrings())
        result.append(addIOModule(ioModuleName, parentServerId));
    return result;
}

QnResourceList ResourceTreeModelSortingGroupingTest::createVirtualCameras(
    const nx::Uuid& parentServerId) const
{
    QnResourceList result;
    for (const auto virtualCameraName: sortingSignificantStrings())
        result.append(addVirtualCamera(virtualCameraName, parentServerId));
    return result;
}

QnLayoutResourceList ResourceTreeModelSortingGroupingTest::createLayouts(
    const nx::Uuid& parentUserId) const
{
    QnLayoutResourceList result;
    for (const auto layoutName: sortingSignificantStrings())
        result.append(addLayout(layoutName, parentUserId));
    return result;
}

} // namespace test
} // namespace nx::vms::client::desktop
