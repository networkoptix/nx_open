#include <gtest/gtest.h>

#include <common/common_module.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_test_helper.h>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <test_support/resource/camera_resource_stub.h>

class QnResourcePoolTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
        initializeContext(m_module.data());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        m_module.clear();
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
};

TEST_F(QnResourcePoolTest, removeCameraFromLayoutById)
{
    auto camera = addCamera();
    auto layout = addLayout();

    QnLayoutItemData item;
    item.uuid = QnUuid::createUuid();
    item.resource.id = camera->getId();
    layout->addItem(item);

    resourcePool()->removeResource(camera);

    ASSERT_TRUE(layout->getItems().empty());
}

TEST_F(QnResourcePoolTest, removeCameraFromLayoutByUniqueId)
{
    auto camera = addCamera();
    auto layout = addLayout();

    QnLayoutItemData item;
    item.uuid = QnUuid::createUuid();
    item.resource.uniqueId = camera->getUniqueId();
    layout->addItem(item);

    resourcePool()->removeResource(camera);

    ASSERT_TRUE(layout->getItems().empty());
}

TEST_F(QnResourcePoolTest, removeLayoutFromVideowall)
{
    auto videoWall = addVideoWall();
    auto layout = addLayoutForVideoWall(videoWall);

    auto layoutIsOnVideowall =
        [&]()
        {
            for (auto item: videoWall->items()->getItems())
            {
                if (item.layout == layout->getId())
                    return true;
            }
            return false;
        };

    ASSERT_TRUE(layoutIsOnVideowall());
    resourcePool()->removeResource(layout);
    ASSERT_FALSE(layoutIsOnVideowall());
}
