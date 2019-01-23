#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <test_support/resource/camera_resource_stub.h>

#include <nx/core/access/access_types.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_item_index.h>

#include <nx/vms/client/desktop/analytics/analytics_objects_visualization_manager.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace {

static const QnUuid kSystemId1("{A85E63C4-D2F2-4CF4-BA6C-C7480ADCDD7F}");
static const QnUuid kSystemId2("{B647474A-8354-4E7F-B98E-D5718B695F55}");

} // namespace

namespace nx::vms::client::desktop {

static constexpr auto kDefaultMode = AnalyticsObjectsVisualizationManager::kDefaultMode;
static constexpr auto kNonDefaultMode = AnalyticsObjectsVisualizationManager::kNonDefaultMode;

using Mode = AnalyticsObjectsVisualizationMode;

void PrintTo(const Mode& val, ::std::ostream* os)
{
    QString mode;
    switch (val)
    {
        case Mode::always: mode = "always"; break;
        case Mode::tabOnly: mode = "tabOnly"; break;
        case Mode::undefined: mode = "undefined"; break;
    }
    *os << mode.toStdString();
}

class AnalyticsObjectsVisualizationManagerTest:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    AnalyticsObjectsVisualizationManagerTest():
        TestWithTemporaryDirectory("nx_vms_client_desktop/analytics_objects_visualization_manager")
    {}

protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        QDir(testDataDir()).removeRecursively();

        m_staticCommon.reset(new QnStaticCommonModule());
        m_module.reset(new QnCommonModule(false, core::access::Mode::direct));
        m_manager.reset(new AnalyticsObjectsVisualizationManager());
        m_manager->setCacheDirectory(testDataDir());
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

        QDir(testDataDir()).removeRecursively();
    }

    QnLayoutItemIndex addCamera()
    {
        const auto camera = new CameraResourceStub();
        resourcePool()->addResource(QnResourcePtr(camera));

        QnLayoutItemData item;
        item.uuid = QnUuid::createUuid();
        item.resource.id = camera->getId();

        m_layout->addItem(item);
        return QnLayoutItemIndex(m_layout, item.uuid);
    }

    QnLayoutItemIndex addZoomWindow()
    {
        auto camera = new CameraResourceStub();
        resourcePool()->addResource(QnResourcePtr(camera));
        camera->setHasDualStreaming(true);

        QnLayoutItemData item;
        item.uuid = QnUuid::createUuid();
        item.resource.id = camera->getId();
        item.zoomRect = QRectF(0.1, 0.1, 0.5, 0.5);

        m_layout->addItem(item);
        return QnLayoutItemIndex(m_layout, item.uuid);
    }

    QnCommonModule* commonModule() const { return m_module.data(); }
    QnResourcePool* resourcePool() const { return commonModule()->resourcePool(); }
    AnalyticsObjectsVisualizationManager* manager() const { return m_manager.data(); }
    QnLayoutResourcePtr layout() const { return m_layout; }

    // Declares the variables your tests want to use.
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnCommonModule> m_module;
    QScopedPointer<AnalyticsObjectsVisualizationManager> m_manager;
    QnLayoutResourcePtr m_layout;
};

TEST_F(AnalyticsObjectsVisualizationManagerTest, setModeToCamera)
{
    QnLayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(items, kNonDefaultMode);
    ASSERT_EQ(kNonDefaultMode, manager()->mode(items));

}

TEST_F(AnalyticsObjectsVisualizationManagerTest, setModeToCameras)
{
    QnLayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(items, kNonDefaultMode);
    ASSERT_EQ(kNonDefaultMode, manager()->mode(items));
}

TEST_F(AnalyticsObjectsVisualizationManagerTest, checkNewCamera)
{
    QnLayoutItemIndexList items;
    items << addCamera();
    ASSERT_EQ(kDefaultMode, manager()->mode(items));
}

TEST_F(AnalyticsObjectsVisualizationManagerTest, setModeToCamerasDifferent)
{
    QnLayoutItemIndexList nonDefaultItems;
    nonDefaultItems << addCamera();
    manager()->setMode(nonDefaultItems, kNonDefaultMode);

    QnLayoutItemIndexList defaultItems;
    defaultItems << addCamera();
    manager()->setMode(defaultItems, kDefaultMode);

    auto allItems = nonDefaultItems + defaultItems;
    ASSERT_EQ(Mode::undefined, manager()->mode(allItems));
}

TEST_F(AnalyticsObjectsVisualizationManagerTest, checkCacheDirectory)
{
    ASSERT_EQ(testDataDir(), manager()->cacheDirectory());
}

TEST_F(AnalyticsObjectsVisualizationManagerTest, switchLocalId)
{
    QnLayoutItemIndexList items;
    items << addCamera();

    manager()->setMode(items, kNonDefaultMode);
    manager()->switchLocalSystemId(kSystemId1);
    ASSERT_EQ(kDefaultMode, manager()->mode(items));
}

TEST_F(AnalyticsObjectsVisualizationManagerTest, saveAndLoadPersistentData)
{
    QnLayoutItemIndexList items;
    items << addCamera();

    manager()->switchLocalSystemId(kSystemId1);
    manager()->setMode(items, kNonDefaultMode);
    manager()->saveData(kSystemId1, resourcePool());
    manager()->switchLocalSystemId(kSystemId2);
    ASSERT_EQ(kDefaultMode, manager()->mode(items));
    manager()->switchLocalSystemId(kSystemId1);
    ASSERT_EQ(kNonDefaultMode, manager()->mode(items));
}

} // namespace nx::vms::client::desktop
