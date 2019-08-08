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
#include <ui/style/resource_icon_cache.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;

class ResourceTreeModelTest: public testing::Test
{
protected:
    virtual void SetUp()
    {
        QnStartupParameters startupParameters;
        startupParameters.skipMediaFolderScan = true;

        m_clientModule.reset(new QnClientModule(startupParameters, nullptr));
        m_accessController.reset(new QnWorkbenchAccessController(commonModule()));
        m_layoutSnapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(commonModule()));
        m_resourceTreeModel.reset(new QnResourceTreeModel(
            QnResourceTreeModel::FullScope,
            m_accessController.get(),
            m_layoutSnapshotManager.get(),
            commonModule()));

        m_resourceTreeProxyModel.reset(new QnResourceTreeSortProxyModel());
        m_resourceTreeProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        m_resourceTreeProxyModel->setSourceModel(m_resourceTreeModel.get());
        m_resourceTreeProxyModel->sort(Qn::NameColumn);
    }

    virtual void TearDown()
    {
        m_resourceTreeProxyModel.clear();
        m_resourceTreeModel.clear();
        m_layoutSnapshotManager.clear();
        m_accessController.clear();
        m_clientModule.clear();
    }

    QnCommonModule* commonModule() const
    {
        return m_clientModule->clientCoreModule()->commonModule();
    }

    QnResourcePool* resourcePool() const
    {
        return commonModule()->resourcePool();
    }

    QnUserResourcePtr addUser(
        const QString& name,
        GlobalPermissions globalPermissions,
        QnUserType userType = QnUserType::Local)
    {
        QnUserResourcePtr user(new QnUserResource(userType));
        user->setId(QnUuid::createUuid());
        user->setName(name);
        user->setRawPermissions(globalPermissions);
        user->addFlags(Qn::remote);
        resourcePool()->addResource(user);
        return user;
    }

    QnMediaServerResourcePtr addServer(const QString& name)
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource(commonModule()));
        server->setId(QnUuid::createUuid());
        server->setStatus(Qn::ResourceStatus::Offline);
        server->setName(name);
        resourcePool()->addResource(server);
        return server;
    }

    void logout()
    {
        m_accessController->setUser(QnUserResourcePtr());
        resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
    }

    void loginAsAdmin(const QString& name)
    {
        logout();
        auto user = addUser(name, GlobalPermission::adminPermissions);
        m_accessController->setUser(user);
    }

    void setSystemName(const QString& name)
    {
        commonModule()->globalSettings()->setSystemName(name);
    }

    QJsonDocument testSnapshot(ItemModelStateSnapshotHelper::SnapshotParams& params) const
    {
        return ItemModelStateSnapshotHelper::makeSnapshot(m_resourceTreeProxyModel.get(), params);
    }

    QModelIndexList getAllIndexes() const
    {
        std::function<QModelIndexList(const QModelIndex&)> getChildren;
        getChildren =
            [this, &getChildren](const QModelIndex& parent)
            {
                QModelIndexList result;
                for (int row = 0; row < m_resourceTreeProxyModel->rowCount(parent); ++row)
                {
                    const auto childIndex = m_resourceTreeProxyModel->index(row, 0, parent);
                    result.append(childIndex);
                    result.append(getChildren(childIndex));
                }
                return result;
            };
        return getChildren(QModelIndex());
    }

    using KeyValue = std::pair<int, QVariant>;
    using KeyValueVector = std::vector<KeyValue>;
    QModelIndexList getIndexByData(const KeyValueVector& lookupData) const
    {
        const auto dataMatch =
            [lookupData](const QModelIndex& index) -> bool
            {
                for (const auto& keyValue: lookupData)
                {
                    const auto indexData = index.data(keyValue.first);
                    if (indexData.isNull() || indexData != keyValue.second)
                        return false;
                }
                return true;
            };

        QModelIndexList result;
        const auto allIndexes = getAllIndexes();
        std::copy_if(allIndexes.cbegin(), allIndexes.cend(), std::back_inserter(result), dataMatch);

        return result;
    }

protected:
    QSharedPointer<QnClientModule> m_clientModule;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnWorkbenchLayoutSnapshotManager> m_layoutSnapshotManager;
    QSharedPointer<QnResourceTreeModel> m_resourceTreeModel;
    QSharedPointer<QnResourceTreeSortProxyModel> m_resourceTreeProxyModel;
};

TEST_F(ResourceTreeModelTest, shouldShowPinnedNodesIfLoggedIn)
{
    // Define string constants.
    static constexpr auto systemName = "test_system";
    static constexpr auto userName = "test_user";

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.roles = {Qt::DisplayRole, Qn::ResourceIconKeyRole};
    params.depth = 0;
    params.rowCount = 3;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "%1",
                    "iconKey": "CurrentSystem"
                }
            },
            {
                "data": {
                    "display": "%2",
                    "iconKey": "User"
                }
            },
            {
                "data": {
                    "iconKey": "Unknown"
                }
            }
        ])json")
        .arg(systemName)
        .arg(userName)
        .toUtf8());

    // Perform actions.
    setSystemName(systemName);
    loginAsAdmin(userName);

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, singleServerShouldBeTopLevelNode)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto serverName1 = "test_server_1";

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.roles = {Qt::DisplayRole, Qn::ResourceIconKeyRole};
    params.depth = 1;
    params.startRow = 3;
    params.rowCount = 1;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "data": {
                    "display": "%1",
                    "iconKey": "Server|Offline"
                }
            }
        ])json")
        .arg(serverName1)
        .toUtf8());

    // Perform actions.
    loginAsAdmin(userName);
    addServer(serverName1);

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, shouldGroupServersIfNotSingle)
{
    // Define string constants.
    static constexpr auto userName = "test_user";
    static constexpr auto serverName1 = "test_server_1";
    static constexpr auto serverName2 = "test_server_2";

    // Define reference snapshot.
    ItemModelStateSnapshotHelper::SnapshotParams params;
    params.roles = {Qt::DisplayRole, Qn::ResourceIconKeyRole};
    params.depth = 1;
    params.startRow = 3;
    params.rowCount = 1;
    const auto referenceSnapshot = QJsonDocument::fromJson(QString(R"json(
        [
            {
                "children": [
                    {
                        "data": {
                            "display": "%1",
                            "iconKey": "Server|Offline"
                        }
                    },
                    {
                        "data": {
                            "display": "%2",
                            "iconKey": "Server|Offline"
                        }
                    }
                ],
                "data": {
                    "display": "Servers",
                    "iconKey": "Servers"
                }
            }
        ])json")
        .arg(serverName1)
        .arg(serverName2)
        .toUtf8());

    // Perform actions.
    loginAsAdmin(userName);
    addServer(serverName1);
    addServer(serverName2);

    // Check result.
    ASSERT_TRUE(testSnapshot(params) == referenceSnapshot);
}

TEST_F(ResourceTreeModelTest, localFilesNodeVisibility)
{
    // Define string constants.
    static constexpr auto userName = "test_user";

    // Define reference data.
    const KeyValueVector lookupData =
        {{Qt::DisplayRole, "Local Files"},
        {Qn::ResourceIconKeyRole, QnResourceIconCache::LocalResources}};

    // Perform actions.

    // TODO: #vbreus On a first sight there should be a zero value, but actual representation of
    // the resource tree depends not only model state, but also on the root node set to the view.
    // Need to figure out how to achieve one to one correspondence of testing environment model and
    // actually displayed hierarchy.
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
    loginAsAdmin(userName);
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
    logout();
    // Same as above.
    ASSERT_TRUE(getIndexByData(lookupData).size() == 1);
}
