#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_module.h>

#include <nx/core/access/access_types.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_data.h>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>

namespace nx {
namespace client {
namespace desktop {

class RadassResourceManagerTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_staticCommon.reset(new QnStaticCommonModule());
        m_module.reset(new QnCommonModule(false, core::access::Mode::direct));
        m_manager.reset(new RadassResourceManager());
        m_layout.reset(new QnLayoutResource());
        m_layout->setId(QnUuid::createUuid());
        resourcePool()->addResource(m_layout);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_layout.reset();
        m_manager.reset();
        m_module.reset();
        m_staticCommon.reset();
    }

    QnUuid addCamera()
    {
        QnLayoutItemData item;
        item.uuid = QnUuid::createUuid();
        m_layout->addItem(item);
        return item.uuid;
    }

    QnCommonModule* commonModule() const { return m_module.data(); }
    QnResourcePool* resourcePool() const { return commonModule()->resourcePool(); }
    RadassResourceManager* manager() const { return m_manager.data(); }
    QnLayoutResourcePtr layout() const { return m_layout; }

    // Declares the variables your tests want to use.
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnCommonModule> m_module;
    QScopedPointer<RadassResourceManager> m_manager;
    QnLayoutResourcePtr m_layout;
};

TEST_F(RadassResourceManagerTest, initNull)
{
    ASSERT_EQ(manager()->mode(QnLayoutResourcePtr()), RadassMode::Auto);
}

TEST_F(RadassResourceManagerTest, initLayout)
{
    ASSERT_EQ(manager()->mode(layout()), RadassMode::Auto);
}

TEST_F(RadassResourceManagerTest, setModeToLayout)
{
    manager()->setMode(layout(), RadassMode::High);
    ASSERT_EQ(manager()->mode(layout()), RadassMode::High);
}


} // namespace desktop
} // namespace client
} // namespace nx
