#pragma once

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <client/client_module.h>
#include <api/global_settings.h>
#include <nx/vms/api/data/layout_tour_data.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource_tree_sort_proxy_model.h>

#include <core/resource/resource_fwd.h>

#include "resource_tree_model_index_condition.h"

namespace nx::vms::client::desktop {

class ResourceTreeModelTest: public testing::Test
{
protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    QnCommonModule* commonModule() const;
    QnResourcePool* resourcePool() const;

    void setSystemName(const QString& name) const;

    QnUserResourcePtr addUser(const QString& name, GlobalPermissions globalPermissions,
        QnUserType userType = QnUserType::Local) const;
    QnMediaServerResourcePtr addServer(const QString& name) const;
    QnAviResourcePtr addMediaResource(const QString& path) const;
    QnFileLayoutResourcePtr addFileLayoutResource(const QString& path,
        bool isEncrypted = false) const;
    QnLayoutResourcePtr addLayoutResource(const QString& name,
        const QnUuid& parentId = QnUuid()) const;
    QnWebPageResourcePtr addWebPageResource(const QString& name) const;
    QnVideoWallResourcePtr addVideoWallResource(const QString& name) const;
    nx::vms::api::LayoutTourData addLayoutTour(const QString& name,
        const QnUuid& parentId = QnUuid()) const;

    void logout() const;
    QnUserResourcePtr loginAsAdmin(const QString& name) const;
    QnUserResourcePtr loginAsLiveViewer(const QString& name) const;

    QAbstractItemModel* model() const;
    QModelIndexList getAllIndexes() const;

    QModelIndex uniqueMatchingIndex(index_condition::Condition condition) const;
    QModelIndexList allMatchingIndexes(index_condition::Condition condition) const;

    bool noneMatches(index_condition::Condition condition) const;
    bool onlyOneMatches(index_condition::Condition condition) const;
    int matchCount(index_condition::Condition condition) const;

    std::vector<QString> transformToDisplayStrings(const QModelIndexList& indexes) const;

protected:
    QSharedPointer<QnClientModule> m_clientModule;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnWorkbenchLayoutSnapshotManager> m_layoutSnapshotManager;
    QSharedPointer<QnResourceTreeModel> m_resourceTreeModel;
    QSharedPointer<QnResourceTreeSortProxyModel> m_resourceTreeProxyModel;
};

} // namespace nx::vms::client::desktop
