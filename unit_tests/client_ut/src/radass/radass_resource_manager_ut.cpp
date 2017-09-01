#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_module.h>

#include <test_support/resource/camera_resource_stub.h>

#include <nx/core/access/access_types.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>

namespace nx {
namespace client {
namespace desktop {


void PrintTo(const RadassMode& val, ::std::ostream* os)
{
    QString mode;
    switch (val)
    {
        case RadassMode::Auto: mode = "auto"; break;
        case RadassMode::High: mode = "high"; break;
        case RadassMode::Low: mode = "low"; break;
        case RadassMode::Custom: mode = "custom"; break;
    }
    *os << mode.toStdString();
}

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

    QnLayoutItemIndex addCamera(bool hasDualStreaming = true)
    {
        auto camera = new CameraResourceStub();
        resourcePool()->addResource(QnResourcePtr(camera));
        camera->setHasDualStreaming(hasDualStreaming);

        QnLayoutItemData item;
        item.uuid = QnUuid::createUuid();
        item.resource.id = camera->getId();

        m_layout->addItem(item);
        return QnLayoutItemIndex(m_layout, item.uuid);
    }

    QnLayoutItemIndex addUnsupportedCamera()
    {
        return addCamera(false);
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
    ASSERT_EQ(RadassMode::Auto, manager()->mode(QnLayoutResourcePtr()));
}

TEST_F(RadassResourceManagerTest, initLayout)
{
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, setModeToEmptyLayout)
{
    manager()->setMode(layout(), RadassMode::High);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, setModeToLayout)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, ignoreCustomModeToLayout)
{
    QnLayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(layout(), RadassMode::Custom);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, ignoreCustomModeToItems)
{
    QnLayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(items, RadassMode::Custom);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToCamera)
{
    QnLayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToUnsupportedCamera)
{
    QnLayoutItemIndexList items;
    items << addUnsupportedCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToCameras)
{
    QnLayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToMixedCameras)
{
    QnLayoutItemIndexList items;
    items << addCamera() << addCamera();

    auto mixedItems = items;
    mixedItems << addUnsupportedCamera();

    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(mixedItems));

    manager()->setMode(items, RadassMode::Low);
    ASSERT_EQ(RadassMode::Low, manager()->mode(mixedItems));
}

TEST_F(RadassResourceManagerTest, setModeToLayoutCheckCameras)
{
    QnLayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(layout(), RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToLayoutWithUnsupportedCamerasOnly)
{
    QnLayoutItemIndexList items;
    items << addUnsupportedCamera() << addUnsupportedCamera();
    manager()->setMode(layout(), RadassMode::High);

    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, checkNewCamera)
{
    QnLayoutItemIndexList items;
    items << addCamera();
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToLayoutCheckNewCamera)
{
    manager()->setMode(layout(), RadassMode::High);

    QnLayoutItemIndexList items;
    items << addCamera();
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToCamerasCheckLayout)
{
    QnLayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, setModeToCamerasDifferent)
{
    QnLayoutItemIndexList highItems;
    highItems << addCamera();
    manager()->setMode(highItems, RadassMode::High);

    QnLayoutItemIndexList lowItems;
    lowItems << addCamera();
    manager()->setMode(lowItems, RadassMode::Low);

    ASSERT_EQ(RadassMode::Custom, manager()->mode(layout()));

    auto allItems = highItems;
    allItems.append(lowItems);
    ASSERT_EQ(RadassMode::Custom, manager()->mode(allItems));
}

TEST_F(RadassResourceManagerTest, addCameraToPresetLayout)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);

    QnLayoutItemIndexList items;
    items << addCamera();

    ASSERT_EQ(RadassMode::Custom, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, addUnsupportedCameraToPresetLayout)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);
    addUnsupportedCamera();
    ASSERT_EQ(RadassMode::High, manager()->mode(layout()));
}

} // namespace desktop
} // namespace client
} // namespace nx
