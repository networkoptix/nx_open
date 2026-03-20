// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/radass/radass_storage.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>

namespace {

static const nx::Uuid kSystemId1("{A85E63C4-D2F2-4CF4-BA6C-C7480ADCDD7F}");
static const nx::Uuid kSystemId2("{B647474A-8354-4E7F-B98E-D5718B695F55}");

} // namespace

namespace nx::vms::client::desktop {

class TestRadassStorage: public AbstractRadassStorage
{
public:
    RadassModeByLayoutItemIdHash localModes() const override
    {
        return m_settings[m_systemId].local;
    }
    void setLocalModes(const RadassModeByLayoutItemIdHash& m) override
    {
        m_settings[m_systemId].local = m;
    }
    RadassModeByLayoutItemIdHash cloudModes() const override
    {
        return m_settings[m_systemId].cloud;
    }
    void setCloudModes(const RadassModeByLayoutItemIdHash& m) override
    {
        m_settings[m_systemId].cloud = m;
    }

    void setSystemId(nx::Uuid id) { m_systemId = id; }

private:
    nx::Uuid m_systemId;

    struct Settings
    {
        RadassModeByLayoutItemIdHash local;
        RadassModeByLayoutItemIdHash cloud;
    };

    mutable std::map<nx::Uuid, Settings> m_settings;
};

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
        auto storage = std::make_unique<TestRadassStorage>();
        m_storage = storage.get();
        m_manager.reset(new RadassResourceManager(std::move(storage)));
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
    }

    void switchLocalSystemId(nx::Uuid id) { m_storage->setSystemId(id); }

    void initCrossSystemLayout()
    {
        m_cloudLayout.reset(new core::CrossSystemLayoutResource());
        m_cloudLayout->setIdUnsafe(nx::Uuid::createUuid());
        nx::vms::client::core::appContext()->cloudLayoutsPool()->addResource(m_cloudLayout);
    }

    LayoutItemIndex addCamera(bool hasDualStreaming = true)
    {
        auto camera = QnResourcePoolTestHelper(systemContext()).addCamera();
        camera->setHasDualStreaming(hasDualStreaming);

        common::LayoutItemData item;
        item.uuid = nx::Uuid::createUuid();
        item.resource.id = camera->getId();

        m_layout->addItem(item);
        return LayoutItemIndex(m_layout, item.uuid);
    }

    LayoutItemIndex addCloudCamera()
    {
        auto camera = QnResourcePoolTestHelper(systemContext()).addCamera();

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
        auto camera = QnResourcePoolTestHelper(systemContext()).addCamera();
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

    TestRadassStorage* m_storage = nullptr;
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

    EXPECT_EQ(RadassMode::Custom, manager()->mode(layout()));
    EXPECT_EQ(RadassMode::Auto, manager()->mode(items));
}

TEST_F(RadassResourceManagerTest, addUnsupportedCameraToPresetLayout)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);
    addUnsupportedCamera();
    addZoomWindow();
    EXPECT_EQ(RadassMode::High, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, switchLocalId)
{
    addCamera();
    manager()->setMode(layout(), RadassMode::High);
    EXPECT_EQ(manager()->mode(layout()), RadassMode::High);

    switchLocalSystemId(kSystemId1);
    EXPECT_EQ(RadassMode::Auto, manager()->mode(layout()));
}

TEST_F(RadassResourceManagerTest, saveAndLoadPersistentData)
{
    initCrossSystemLayout();
    switchLocalSystemId(kSystemId1);
    addCamera();
    addCloudCamera();

    manager()->setMode(layout(), RadassMode::High);
    manager()->setMode(cloudLayout(), RadassMode::High);
    switchLocalSystemId(kSystemId2);
    EXPECT_EQ(RadassMode::Auto, manager()->mode(layout()));
    EXPECT_EQ(RadassMode::Auto, manager()->mode(cloudLayout()));

    switchLocalSystemId(kSystemId1);
    EXPECT_EQ(RadassMode::High, manager()->mode(layout()));
    EXPECT_EQ(RadassMode::High, manager()->mode(cloudLayout()));
}

} // namespace nx::vms::client::desktop
