// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <thread>

#include <api/helpers/camera_id_helper.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/utils/random.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>

class QnResourcePoolTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_staticCommon = std::make_unique<QnStaticCommonModule>();
        m_module = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::direct);
        initializeContext(m_module.get());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        m_module.reset();
        m_staticCommon.reset();
    }

    // Declares the variables your tests want to use.
    std::unique_ptr<QnStaticCommonModule> m_staticCommon;
    std::unique_ptr<QnCommonModule> m_module;
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

TEST_F(QnResourcePoolTest, findCameraTest)
{
    static constexpr const size_t kCameraCount = 100;
    static constexpr const size_t kThreadCount = 4;
    static constexpr const size_t kRequestCount = 1000;
    NX_INFO(this, "Testing on %1 cameras, %2 threads, %3 requests",
        kCameraCount, kThreadCount, kRequestCount);

    std::vector<nx::CameraResourceStubPtr> cameras = addCameras(kCameraCount);
    for (size_t i = 0; i < cameras.size(); ++i)
        cameras[i]->setLogicalId(i + 1);

    const auto runThreads =
        [&](const QString& label, bool expectSuccess, const auto& getter)
        {
            using namespace std::chrono;
            const auto time = steady_clock::now();
            std::vector<std::thread> threads;
            for (size_t i = 0; i < kThreadCount; ++i)
            {
                threads.push_back(std::thread(
                    [&]
                    {
                        for (size_t i = 0; i < kRequestCount; ++i)
                        {
                            const auto camera = nx::utils::random::choice(cameras);
                            EXPECT_EQ(getter(camera).data() == camera.data(), expectSuccess);
                        }
                    }));
            }
            for (auto& t: threads)
                t.join();
            const auto duration = duration_cast<milliseconds>(steady_clock::now() - time);
            NX_INFO(this, "%1: %2", label, duration);
        };

    runThreads("id", /*expectSuccess*/ true,
        [&](auto c) { return resourcePool()->getResourceById(c->getId()); });
    runThreads("uniqueId", /*expectSuccess*/ true,
        [&](auto c) { return resourcePool()->getResourceByUniqueId(c->getUniqueId()); });
    runThreads("physicalId", /*expectSuccess*/ true,
        [&](auto c) { return resourcePool()->getNetResourceByPhysicalId(c->getPhysicalId()); });
    runThreads("logicalId", /*expectSuccess*/ true,
        [&](auto c) { return resourcePool()->getResourcesByLogicalId(c->logicalId()).at(0); });
    runThreads("MAC", /*expectSuccess*/ true,
        [&](auto c) { return resourcePool()->getResourceByMacAddress(c->getMAC().toString()); });

    using namespace nx::camera_id_helper;
    runThreads("flexibleId id", /*expectSuccess*/ true,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), c->getId().toString()); });
    runThreads("flexibleId simple id", /*expectSuccess*/ true,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), c->getId().toSimpleString()); });
    runThreads("flexibleId uniqueId", /*expectSuccess*/ true,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), c->getUniqueId()); });
    runThreads("flexibleId physicalId", /*expectSuccess*/ true,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), c->getPhysicalId()); });
    runThreads("flexibleId logicalId", /*expectSuccess*/ true,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), nx::toString(c->logicalId())); });
    runThreads("flexibleId MAC", /*expectSuccess*/ true,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), c->getMAC().toString()); });
    runThreads("flexibleId wrong id", /*expectSuccess*/ false,
        [&](auto c) { return findCameraByFlexibleId(resourcePool(), "no_such_id"); });
}
