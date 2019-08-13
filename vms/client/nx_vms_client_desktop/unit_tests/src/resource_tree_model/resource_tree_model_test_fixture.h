#pragma once

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <client/client_module.h>
#include <api/global_settings.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource_tree_sort_proxy_model.h>
#include <ui/models/item_model_state_snapshot_helper.h>

#include <core/resource/resource_fwd.h>

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

    void logout() const;
    QnUserResourcePtr loginAsAdmin(const QString& name) const;
    QnUserResourcePtr loginAsLiveViewer(const QString& name) const;

    using KeyValue = std::pair<int, QVariant>;
    using KeyValueVector = std::vector<KeyValue>;
    QAbstractItemModel* model() const;
    QModelIndexList getAllIndexes() const;
    QModelIndexList getIndexByData(const KeyValueVector& lookupData) const;

    QJsonDocument testSnapshot(ItemModelStateSnapshotHelper::SnapshotParams& params) const;
    std::string snapshotsOutputString(const QJsonDocument& actual,
        const QJsonDocument& reference) const;

protected:
    QSharedPointer<QnClientModule> m_clientModule;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnWorkbenchLayoutSnapshotManager> m_layoutSnapshotManager;
    QSharedPointer<QnResourceTreeModel> m_resourceTreeModel;
    QSharedPointer<QnResourceTreeSortProxyModel> m_resourceTreeProxyModel;
};

} // namespace nx::vms::client::desktop
