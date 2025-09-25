// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace {

static const nx::Uuid kSystemId1("{A85E63C4-D2F2-4CF4-BA6C-C7480ADCDD7F}");
static const nx::Uuid kSystemId2("{B647474A-8354-4E7F-B98E-D5718B695F55}");

} // namespace

namespace nx::vms::client::desktop {

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

struct WithCrossSiteFeatures
{
    static ApplicationContext::Features features()
    {
        auto result = ApplicationContext::Features::none();
        result.core.base.flags.setFlag(common::ApplicationContext::FeatureFlag::networking);
        result.core.flags.setFlag(core::ApplicationContext::FeatureFlag::cross_site);
        return result;
    }
};

class RadassResourceManagerTest:
    public test::AppContextBasedTest<WithCrossSiteFeatures>,
    public nx::utils::test::TestWithTemporaryDirectory
{
protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        QDir(testDataDir()).removeRecursively();

        m_manager.reset(new RadassResourceManager());
        m_manager->setCacheDirectory(testDataDir());
        m_layout.reset(new core::LayoutResource());
        m_layout->setIdUnsafe(nx::Uuid::createUuid());
        systemContext()->resourcePool()->addResource(m_layout);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_layout.reset();
        m_cloudLayout.reset();
        m_manager.reset();

        QDir(testDataDir()).removeRecursively();
    }

    void initCrossSytemLayout()
    {
        m_cloudLayout.reset(new core::CrossSystemLayoutResource());
        m_cloudLayout->setIdUnsafe(nx::Uuid::createUuid());
        nx::vms::client::core::appContext()->cloudLayoutsPool()->addResource(m_cloudLayout);
    }

    LayoutItemIndex addCamera(bool hasDualStreaming = true)
    {
        auto camera = new CameraResourceStub();
        systemContext()->resourcePool()->addResource(QnResourcePtr(camera));
        camera->setHasDualStreaming(hasDualStreaming);

        common::LayoutItemData item;
        item.uuid = nx::Uuid::createUuid();
        item.resource.id = camera->getId();

        m_layout->addItem(item);
        return LayoutItemIndex(m_layout, item.uuid);
    }

    LayoutItemIndex addCloudCamera()
    {
        auto camera = new CameraResourceStub();
        systemContext()->resourcePool()->addResource(QnResourcePtr(camera));

        common::LayoutItemData item;
        item.uuid = nx::Uuid::createUuid();
        item.resource.id = camera->getId();
        item.resource.path = "VALUE SHOULDN'T BE EMPTY";

        NX_ASSERT(m_cloudLayout);
        m_cloudLayout->addItem(item);
        return LayoutItemIndex(m_cloudLayout, item.uuid);
    }

    LayoutItemIndex addUnsupportedCamera()
    {
        return addCamera(false);
    }

    LayoutItemIndex addZoomWindow()
    {
        auto camera = new CameraResourceStub();
        systemContext()->resourcePool()->addResource(QnResourcePtr(camera));
        camera->setHasDualStreaming(true);

        common::LayoutItemData item;
        item.uuid = nx::Uuid::createUuid();
        item.resource.id = camera->getId();
        item.zoomRect = QRectF(0.1, 0.1, 0.5, 0.5);

        m_layout->addItem(item);
        return LayoutItemIndex(m_layout, item.uuid);
    }

    RadassResourceManager* manager() const { return m_manager.data(); }
    core::LayoutResourcePtr layout() const { return m_layout; }
    core::CrossSystemLayoutResourcePtr cloudLayout() const { return m_cloudLayout; }

    QScopedPointer<RadassResourceManager> m_manager;
    core::LayoutResourcePtr m_layout;
    core::CrossSystemLayoutResourcePtr m_cloudLayout;
};

TEST_F(RadassResourceManagerTest, initNull)
{
    ASSERT_EQ(RadassMode::Auto, manager()->mode(core::LayoutResourcePtr()));
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
    LayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(layout(), RadassMode::Custom);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, ignoreCustomModeToItems)
{
    LayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(items, RadassMode::Custom);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToCamera)
{
    LayoutItemIndexList items;
    items << addCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToUnsupportedCamera)
{
    LayoutItemIndexList items;
    items << addUnsupportedCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToFisheyeItem)
{
    LayoutItemIndexList items;
    items << addZoomWindow();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToCameras)
{
    LayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToMixedCameras)
{
    LayoutItemIndexList items;
    items << addCamera() << addCamera();

    auto mixedItems = items;
    mixedItems << addUnsupportedCamera();
    mixedItems << addZoomWindow();

    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(mixedItems));

    manager()->setMode(items, RadassMode::Low);
    ASSERT_EQ(RadassMode::Low, manager()->mode(mixedItems));
}

TEST_F(RadassResourceManagerTest, setModeToLayoutCheckCameras)
{
    LayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(layout(), RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToLayoutWithUnsupportedCamerasOnly)
{
    LayoutItemIndexList items;
    items << addUnsupportedCamera() << addZoomWindow();
    manager()->setMode(layout(), RadassMode::High);

    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, checkNewCamera)
{
    LayoutItemIndexList items;
    items << addCamera();
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToLayoutCheckNewCamera)
{
    manager()->setMode(layout(), RadassMode::High);

    LayoutItemIndexList items;
    items << addCamera();
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, setModeToCamerasCheckLayout)
{
    LayoutItemIndexList items;
    items << addCamera() << addCamera();
    manager()->setMode(items, RadassMode::High);
    ASSERT_EQ(RadassMode::High, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, setModeToCamerasDifferent)
{
    LayoutItemIndexList highItems;
    highItems << addCamera();
    manager()->setMode(highItems, RadassMode::High);

    LayoutItemIndexList lowItems;
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

    LayoutItemIndexList items;
    items << addCamera();

    ASSERT_EQ(RadassMode::Custom, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, addUnsupportedCameraToPresetLayout)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);
    addUnsupportedCamera();
    addZoomWindow();
    ASSERT_EQ(RadassMode::High, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, checkCacheDirectory)
{
    ASSERT_EQ(testDataDir(), manager()->cacheDirectory());
}

TEST_F(RadassResourceManagerTest, switchLocalId)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);
    manager()->switchLocalSystemId(kSystemId1);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, saveAndLoadPersistentData)
{
    initCrossSytemLayout();
    manager()->switchLocalSystemId(kSystemId1);
    addCamera();
    addCloudCamera();
    manager()->setMode(layout(), RadassMode::High);
    manager()->setMode(cloudLayout(), RadassMode::High);
    manager()->saveData(kSystemId1, systemContext()->resourcePool());
    manager()->switchLocalSystemId(kSystemId2);
    ASSERT_EQ(RadassMode::Auto, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::Auto, manager()->mode(cloudLayout()));
    manager()->switchLocalSystemId(kSystemId1);
    ASSERT_EQ(RadassMode::High, manager()->mode(layout()));
    ASSERT_EQ(RadassMode::High, manager()->mode(cloudLayout()));
}

} // namespace nx::vms::client::desktop
