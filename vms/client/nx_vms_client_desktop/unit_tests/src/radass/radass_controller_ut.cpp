// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/radass/radass_controller_params.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/state/mock_shared_memory_interface.h>
#include <nx/vms/client/desktop/test_support/test_cam_display.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/client/desktop/test_support/test_timers.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {
using namespace radass;

void PrintTo(RadassMode value, ::std::ostream* os)
{
    switch (value)
    {
        case RadassMode::Auto:
            *os << "Auto";
            break;
        case RadassMode::Low:
            *os << "Low";
            break;
        case RadassMode::High:
            *os << "High";
            break;
        case RadassMode::Custom:
            *os << "Custom";
            break;
    }
}

namespace test {

class RadassControllerTest: public ContextBasedTest
{
public:
    TestTimerFactoryPtr timerFactory{new TestTimerFactory()};
    std::vector<TestCamDisplayPtr> cameras;
    std::unique_ptr<RadassController> controller;

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_sharedMemory = makeMockSharedMemory();
        m_processesMap = makeMockProcessesMap();
        auto sharedMemoryManager = makeMockSharedMemoryManager(m_sharedMemory, m_processesMap);
        appContext()->setSharedMemoryManager(std::move(sharedMemoryManager));
        controller = std::make_unique<RadassController>(timerFactory);
    }

    void givenCameras(int count = 3)
    {
        for (int i = 0; i < count; i++)
        {
            TestCamDisplayPtr camDisplay(new TestCamDisplay());
            camDisplay->name = QString("Camera %1").arg(i);
            camDisplay->cameraID = nx::Uuid::createUuid();
            controller->registerConsumer(camDisplay.data());
            cameras.push_back(camDisplay);
        }
    }

    void passIteration()
    {
        timerFactory->passTime(kTimerInterval * 2);
    }

private:
    std::unique_ptr<MockClientProcessExecutionInterface> m_clientProcessExecutionInterface;
    SharedMemoryDataPtr m_sharedMemory;
    RunningProcessesMap m_processesMap;
};

// Check that all cameras after kMaximumConsumersCount are set to LQ.
TEST_F(RadassControllerTest, AddingCamerasTest)
{
    givenCameras(kMaximumConsumersCount + 5);

    passIteration();
    for (size_t i = 0; i < cameras.size(); i++)
    {
        ASSERT_EQ(cameras[i]->qualityValue,
            i < kMaximumConsumersCount ? MEDIA_Quality_High : MEDIA_Quality_Low);
    };
}

// Check that after some time all extra cameras become HQ.
TEST_F(RadassControllerTest, AllCamerasToHQTest)
{
    givenCameras(kMaximumConsumersCount + 5);

    timerFactory->passTime(kAdditionalHQRetryCounter * kTimerInterval + 5s * cameras.size());
    for (size_t i = 0; i < cameras.size(); i++)
    {
        ASSERT_EQ(cameras[i]->qualityValue, MEDIA_Quality_High);
    };
}

// Check that if camera height became less than 172, go to LQ.
TEST_F(RadassControllerTest, SmallDisplayCameraToLQ)
{
    givenCameras();

    passIteration();
    cameras[1]->videoSize = {1920, 170};
    cameras[1]->maxScreenSize = { 1920, 170 };

    timerFactory->passTime(kTimerInterval * 200);
    ASSERT_EQ(cameras[1]->qualityValue, MEDIA_Quality_Low);
}

// Check that if camera height became less than 172, go to LQ.
TEST_F(RadassControllerTest, CheckDataStarvationToLQ)
{
    givenCameras();

    passIteration();
    cameras[1]->videoSize = { 1920, 170 };
    cameras[1]->maxScreenSize = { 1920, 170 };

    timerFactory->passTime(kTimerInterval * 200);
    ASSERT_EQ(cameras[1]->qualityValue, MEDIA_Quality_Low);
}

// Tests from JIRA (Meta FT-390)
// Current state: Implementation is stopped due to inconsistencies
// between specifications, actual RADASS code and test case descriptions.

// FT - 391: Set manual resolution for cameras - CANNOT BE TESTED

// FT - 392: Live video->Camera is always in Hi - Res in full screen

TEST_F(RadassControllerTest, Fullscreencamera_FT392_C20113)
{
    givenCameras();

    passIteration();
    controller->setMode(cameras[1].data(), RadassMode::Low);
    cameras[1]->fullScreen = true;
    passIteration();
    ASSERT_EQ(cameras[1]->qualityValue, MEDIA_Quality_High);

    // This test is consistent with current RADASS implementation  and not consistent with C20113.
    // The test description is also wrong - actually we need to emulate onSlowStream() as well.
    cameras[1]->speed = 16;
    controller->onSlowStream(cameras[1].data());
    passIteration();
    timerFactory->passTime(1000s);
    ASSERT_EQ(cameras[1]->qualityValue, MEDIA_Quality_High);

    controller->streamBackToNormal(cameras[1].data());
    passIteration();
    ASSERT_EQ(cameras[1]->qualityValue, MEDIA_Quality_High);

    cameras[1]->fullScreen = false;
    passIteration();
    ASSERT_EQ(cameras[1]->qualityValue, MEDIA_Quality_Low);
}

/**
 * Issue description:
 *
 * Open camera  on Scene
 * Right click on camera -> Resolution -> Low
 * Open Camera Settings -> Expert -> Disable 2d stream
 * Open Camera Settings -> Expert -> Enable 2d stream
 * Expected: Resolution should change to Low
 * Actual: Resolution remains in High
 */
TEST_F(RadassControllerTest, CameraStreamsDisabling)
{
    givenCameras(/*count*/ 1);
    auto camera = cameras[0].data();

    controller->setMode(camera, RadassMode::Low);
    passIteration();
    EXPECT_EQ(camera->qualityValue, MEDIA_Quality_Low);

    camera->setDualStreamingEnabled(false);
    passIteration();
    EXPECT_EQ(controller->mode(camera), RadassMode::Custom);

    camera->setDualStreamingEnabled(true);
    passIteration();
    EXPECT_EQ(controller->mode(camera), RadassMode::Low);
    EXPECT_EQ(camera->qualityValue, MEDIA_Quality_Low);
}

} // namespace test
} // namespace nx::vms::client::desktop
