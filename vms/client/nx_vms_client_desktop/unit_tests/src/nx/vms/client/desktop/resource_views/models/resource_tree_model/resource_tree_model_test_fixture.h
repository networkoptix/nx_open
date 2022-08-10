// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <core/resource/client_resource_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/layout_tour_data.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_composer.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

#include "resource_tree_model_index_condition.h"

class QnWorkbenchAccessController;
class QnResourceAccessManager;
class QnRuntimeInfoManager;

namespace nx::vms::client::desktop {

class SystemContext;
class LayoutSnapshotManager;

namespace entity_item_model { class EntityItemModel; }
namespace entity_resource_tree { class ResourceTreeComposer; };

namespace test {

class ResourceTreeModelTest: public nx::vms::client::desktop::test::ContextBasedTest
{
protected:
    using NodeType = ResourceTree::NodeType;

    virtual void SetUp() override;
    virtual void TearDown() override;

    QnResourcePool* resourcePool() const;
    QnWorkbenchAccessController* workbenchAccessController() const;
    QnResourceAccessManager* resourceAccessManager() const;
    LayoutSnapshotManager* layoutSnapshotManager() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;

    void setSystemName(const QString& name) const;

    QnUserResourcePtr addUser(const QString& name, GlobalPermissions globalPermissions,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local) const;
    QnMediaServerResourcePtr addServer(const QString& name) const;
    QnMediaServerResourcePtr addEdgeServer(const QString& name, const QString& address) const;
    QnMediaServerResourcePtr addFakeServer(const QString& name) const;
    QnAviResourcePtr addLocalMedia(const QString& path) const;
    QnFileLayoutResourcePtr addFileLayout(const QString& path,
        bool isEncrypted = false) const;
    QnLayoutResourcePtr addLayout(const QString& name,
        const QnUuid& parentId = QnUuid()) const;
    QnWebPageResourcePtr addWebPage(const QString& name) const;
    QnWebPageResourcePtr addProxiedWebResource(
        const QString& name,
        const QnUuid& serverId) const;

    QnVideoWallResourcePtr addVideoWall(const QString& name) const;
    QnVideoWallItem addVideoWallScreen(
        const QString& name,
        const QnVideoWallResourcePtr& videoWall) const;
    QnVideoWallMatrix addVideoWallMatrix(
        const QString& name,
        const QnVideoWallResourcePtr& videoWall) const;
    void updateVideoWallScreen(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& screen) const;
    void updateVideoWallMatrix(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallMatrix& matrix) const;

    nx::vms::api::LayoutTourData addLayoutTour(const QString& name,
        const QnUuid& parentId = QnUuid()) const;
    QnVirtualCameraResourcePtr addCamera(
        const QString& name,
        const QnUuid& parentId = QnUuid(),
        const QString& hostAddress = QString()) const;
    QnVirtualCameraResourcePtr addEdgeCamera(
        const QString& name, const QnMediaServerResourcePtr& edgeServer) const;
    QnVirtualCameraResourcePtr addVirtualCamera(const QString& name,
        const QnUuid& parentId = QnUuid()) const;
    QnVirtualCameraResourcePtr addIOModule(const QString& name,
        const QnUuid& parentId = QnUuid()) const;
    QnVirtualCameraResourcePtr addRecorderCamera(const QString& name,
        const QString& groupId, const QnUuid& parentId = QnUuid()) const;
    QnVirtualCameraResourcePtr addMultisensorSubCamera(const QString& name,
        const QString& groupId, const QnUuid& parentId = QnUuid()) const;
    QnVirtualCameraResourcePtr addIntercomCamera(
        const QString& name,
        const QnUuid& parentId = QnUuid(),
        const QString& hostAddress = QString()) const;
    QnLayoutResourcePtr addIntercomLayout(
        const QString& name,
        const QnUuid& parentId = QnUuid()) const;
    void removeCamera(const QnVirtualCameraResourcePtr& camera) const;

    void addToLayout(const QnLayoutResourcePtr& layout, const QnResourceList& resources) const;
    void setVideoWallScreenRuntimeStatus(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& videoWallScreen,
        bool isOnline,
        const QnUuid& controlledBy = QnUuid());

    void setupAccessToResourceForUser(
        const QnUserResourcePtr& user,
        const QnResourcePtr& resource,
        bool isAccessible) const;

    QnUserResourcePtr loginAsUserWithPermissions(const QString& name,
        nx::vms::api::GlobalPermissions permissions,
        bool isOwner = false) const;
    QnUserResourcePtr loginAsOwner(const QString& name) const;
    QnUserResourcePtr loginAsAdmin(const QString& name) const;
    QnUserResourcePtr loginAsLiveViewer(const QString& name) const;
    QnUserResourcePtr loginAsAdvancedViewer(const QString& name) const;
    QnUserResourcePtr loginAsCustomUser(const QString& name) const;
    void logout() const;

    void setFilterMode(entity_resource_tree::ResourceTreeComposer::FilterMode filterMode);

    QAbstractItemModel* model() const;
    QModelIndexList getAllIndexes() const;

    QModelIndex firstMatchingIndex(index_condition::Condition condition) const;
    QModelIndex uniqueMatchingIndex(index_condition::Condition condition) const;
    QModelIndexList allMatchingIndexes(index_condition::Condition condition) const;

    bool noneMatches(index_condition::Condition condition) const;
    bool onlyOneMatches(index_condition::Condition condition) const;
    bool atLeastOneMatches(index_condition::Condition condition) const;
    int matchCount(index_condition::Condition condition) const;

    std::vector<QString> transformToDisplayStrings(const QModelIndexList& indexes) const;

protected:
    QSharedPointer<entity_item_model::EntityItemModel> m_newResourceTreeModel;
    QSharedPointer<entity_resource_tree::ResourceTreeComposer> m_resourceTreeComposer;
};

} // namespace test
} // namespace nx::vms::client::desktop
