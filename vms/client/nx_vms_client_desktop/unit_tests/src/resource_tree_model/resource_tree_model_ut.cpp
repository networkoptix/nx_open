#include <gtest/gtest.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>
#include <client/client_module.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource_search_proxy_model.h>

class ResourceTreeModelTest: public testing::Test
{
protected:
    virtual void SetUp()
    {
        m_clientModule.reset(new QnClientModule(QnStartupParameters(), nullptr));
        m_accessController.reset(new QnWorkbenchAccessController(commonModule()));
        m_resourceTreeModel.reset(new QnResourceTreeModel(QnResourceTreeModel::FullScope,
            QnUserResourcePtr(), m_accessController.get(), nullptr, commonModule()));
        m_resourceSearchProxyModel.reset(new QnResourceSearchProxyModel());
        m_resourceSearchProxyModel->setSourceModel(m_resourceTreeModel.get());
    }

    virtual void TearDown()
    {
        m_resourceSearchProxyModel.clear();
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

protected:
    QSharedPointer<QnClientModule> m_clientModule;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnResourceTreeModel> m_resourceTreeModel;
    QSharedPointer<QnResourceSearchProxyModel> m_resourceSearchProxyModel;
};

TEST_F(ResourceTreeModelTest, shouldBeNotEmpty)
{
    ASSERT_FALSE(m_resourceSearchProxyModel->rowCount() == 0);
}
