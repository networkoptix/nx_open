// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_tree_model_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

class ResourceTreeModelSortingGroupingTest: public ResourceTreeModelTest
{
    using base_class = ResourceTreeModelTest;
public:
    virtual void SetUp() override;

protected:
    QStringList sortingSignificantStrings() const;
    bool sortingIsAlphanumericCaseInsensitive(index_condition::Condition condition) const;
    bool sortingIsAlphanumericCaseInsensitive(const QModelIndexList& indexes) const;
    bool checkGrouping(const std::vector<index_condition::Condition>& conditionsInOrder) const;

protected:
    void createAllKindsOfResources(const QnUuid& parentUserId) const;
    void createShowreels(const QnUuid& parentUserId) const;
    void createVideoWallsWithScreensAndMatrices() const;
    void createFileLayouts() const;
    void createSharedLayouts() const;
    void createRecorders(const QnUuid& parentServerId) const;
    void createMultisensorCameras(const QnUuid& parentServerId) const;
    QnResourceList createIncompatibleServers() const;
    QnResourceList createCompatibleServers() const;
    QnResourceList createWebPages() const;
    QnResourceList createLocalImages() const;
    QnResourceList createLocalVideos() const;
    QnResourceList createCameras(const QnUuid& parentServerId) const;
    QnResourceList createIoModules(const QnUuid& parentServerId) const;
    QnResourceList createVirtualCameras(const QnUuid& parentServerId) const;
    QnResourceList createProxiedWebResources(const QnUuid& parentServerId) const;
    QnLayoutResourceList createLayouts(const QnUuid& parentUserId) const;
};

} // namespace test
} // namespace nx::vms::client::desktop
