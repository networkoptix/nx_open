// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <gtest/gtest.h>

#include <core/resource/client_resource_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_composer.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

#include "resource_tree_model_index_condition.h"

class QnResourceAccessManager;
class QnRuntimeInfoManager;

namespace nx::vms::api { enum class WebPageSubtype; }
namespace nx::vms::client::core { class AccessController; }

namespace nx::vms::client::desktop {

class SystemContext;
class LayoutSnapshotManager;

namespace entity_item_model { class EntityItemModel; }
namespace entity_resource_tree { class ResourceTreeComposer; };

namespace test {

class ResourceTreeModelTest:
    public nx::vms::client::desktop::test::ContextBasedTest,
    public ::testing::WithParamInterface<nx::vms::api::WebPageSubtype>
{
protected:
    using NodeType = ResourceTree::NodeType;
    using WebPageSubtype = nx::vms::api::WebPageSubtype;
    using AccessController = nx::vms::client::core::AccessController;

    virtual void SetUp() override;
    virtual void TearDown() override;

    QnResourcePool* resourcePool() const;
    AccessController* accessController() const;
    QnResourceAccessManager* resourceAccessManager() const;
    LayoutSnapshotManager* layoutSnapshotManager() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;

    void setSystemName(const QString& name) const;

    QnUserResourcePtr addUser(const QString& name,
        const std::optional<QnUuid>& groupId = std::nullopt) const;
    ServerResourcePtr addServer(const QString& name) const;
    QnMediaServerResourcePtr addEdgeServer(const QString& name, const QString& address) const;
    void addOtherServer(const QString& name) const;
    QnAviResourcePtr addLocalMedia(const QString& path) const;
    QnFileLayoutResourcePtr addFileLayout(const QString& path,
        bool isEncrypted = false) const;
    LayoutResourcePtr addLayout(const QString& name,
        const QnUuid& parentId = QnUuid()) const;

    QnWebPageResourcePtr addWebPage(const QString& name, WebPageSubtype subtype) const;
    QnWebPageResourcePtr addProxiedWebPage(
        const QString& name,
        WebPageSubtype subtype,
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

    nx::vms::api::ShowreelData addShowreel(const QString& name,
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
    LayoutResourcePtr addIntercomLayout(
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

    void setupAccessToResourceForUser(
        const QnUserResourcePtr& user,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRights accessRights) const;

    void setupAccessToObjectForUser(
        const QnUserResourcePtr& user,
        const QnUuid& resourceOrGroupId,
        nx::vms::api::AccessRights accessRights) const;

    void setupAllMediaAccess(
        const QnUserResourcePtr& user, nx::vms::api::AccessRights accessRights) const;

    void setupControlAllVideoWallsAccess(const QnUserResourcePtr& user) const;

    QnUserResourcePtr loginAs(const QString& name,
        const std::optional<QnUuid>& groupId = std::nullopt) const;
    QnUserResourcePtr loginAsAdministrator(const QString& name) const;
    QnUserResourcePtr loginAsPowerUser(const QString& name) const;
    QnUserResourcePtr loginAsLiveViewer(const QString& name) const;
    QnUserResourcePtr loginAsAdvancedViewer(const QString& name) const;
    QnUserResourcePtr loginAsCustomUser(const QString& name) const;
    QnUserResourcePtr currentUser() const;
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
