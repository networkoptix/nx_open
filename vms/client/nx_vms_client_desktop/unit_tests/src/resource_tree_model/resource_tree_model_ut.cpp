#include <gtest/gtest.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>
#include <client/client_module.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource_tree_sort_proxy_model.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

class ResourceTreeModelTest: public testing::Test
{
protected:
    virtual void SetUp()
    {
        m_clientModule.reset(new QnClientModule(QnStartupParameters(), nullptr));
        m_accessController.reset(new QnWorkbenchAccessController(commonModule()));
        m_resourceTreeModel.reset(new QnResourceTreeModel(QnResourceTreeModel::FullScope,
            QnUserResourcePtr(), m_accessController.get(), nullptr, commonModule()));

        m_resourceTreeProxyModel.reset(new QnResourceTreeSortProxyModel());
        m_resourceTreeProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        m_resourceTreeProxyModel->setSourceModel(m_resourceTreeModel.get());
    }

    virtual void TearDown()
    {
        m_resourceTreeProxyModel.clear();
        m_resourceTreeModel.clear();
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

protected:
    QSharedPointer<QnClientModule> m_clientModule;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnResourceTreeModel> m_resourceTreeModel;
    QSharedPointer<QnResourceTreeSortProxyModel> m_resourceTreeProxyModel;
};

TEST_F(ResourceTreeModelTest, shouldBeNotEmpty)
{
    ASSERT_FALSE(m_resourceTreeProxyModel->rowCount() == 0);
}
