// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <vector>
#include <chrono>

#include <QtCore/QSharedPointer>

#include <nx/utils/uuid.h>

#include <nx/vms/client/desktop/radass/radass_types.h>

#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/radass/radass_controller_params.h>

#include <nx/vms/client/desktop/test_support/test_timers.h>
#include <nx/vms/client/desktop/test_support/test_cam_display.h>

using namespace std::chrono_literals;
using namespace nx::vms::client::desktop::radass;

namespace nx::vms::client::desktop::test {

typedef std::vector<QSharedPointer<TestCamDisplay>> CamDisplaySet;

CamDisplaySet createCamDisplaySet(RadassController& controller, int number,
    TestCamDisplay defaultValue = {}, int startNumber = 0)
{
    CamDisplaySet result(number);
    for (int i = 0; i < number; i++)
    {
        TestCamDisplay* camDisplay = new TestCamDisplay();
        *camDisplay = defaultValue;
        camDisplay->name = QString("Camera %1").arg(i);
        camDisplay->cameraID = QnUuid::createUuid();
        controller.registerConsumer(camDisplay);
        result[i].reset(camDisplay);
    }
    return result;
}

struct TestSetup
{
    TestTimerFactory* timerFactory;
    RadassController controller;
    CamDisplaySet cameras;

    TestSetup(int cams = 3):
        timerFactory(new TestTimerFactory()),
        controller(TimerFactoryPtr{timerFactory}),
        cameras(createCamDisplaySet(controller, cams))
    {
    }

    void passIteration()
    {
        timerFactory->passTime(kTimerInterval * 2);
    }

};
// Check that all cameras after kMaximumConsumersCount are set to LQ.
TEST(RadassControllerTest, AddingCamerasTest)
{
    TestSetup S(kMaximumConsumersCount + 5);

    S.passIteration();
    for (size_t i = 0; i < S.cameras.size(); i++)
    {
        ASSERT_EQ(S.cameras[i]->qualityValue,
            i < kMaximumConsumersCount ? MEDIA_Quality_High : MEDIA_Quality_Low);
    };
}

// Check that after some time all extra cameras become HQ.
TEST(RadassControllerTest, AllCamerasToHQTest)
{
    TestSetup S(kMaximumConsumersCount + 5);

    S.timerFactory->passTime(kAdditionalHQRetryCounter * kTimerInterval + 5s * S.cameras.size());
    for (size_t i = 0; i < S.cameras.size(); i++)
    {
        ASSERT_EQ(S.cameras[i]->qualityValue, MEDIA_Quality_High);
    };
}

// Check that if camera height became less than 172, go to LQ.
TEST(RadassControllerTest, SmallDisplayCameraToLQ)
{
    TestSetup S;

    S.passIteration();
    S.cameras[1]->videoSize = {1920, 170};
    S.cameras[1]->maxScreenSize = { 1920, 170 };

    S.timerFactory->passTime(kTimerInterval * 200);
    ASSERT_EQ(S.cameras[1]->qualityValue, MEDIA_Quality_Low);
}

// Check that if camera height became less than 172, go to LQ.
TEST(RadassControllerTest, CheckDataStarvationToLQ)
{
    TestSetup S;

    S.passIteration();
    S.cameras[1]->videoSize = { 1920, 170 };
    S.cameras[1]->maxScreenSize = { 1920, 170 };

    S.timerFactory->passTime(kTimerInterval * 200);
    ASSERT_EQ(S.cameras[1]->qualityValue, MEDIA_Quality_Low);
}

// Tests from JIRA (Meta FT-390)
// Current state: Implementation is stopped due to inconsistencies
// between specifications, actual RADASS code and test case descriptions.

// FT - 391: Set manual resolution for cameras - CANNOT BE TESTED

// FT - 392: Live video->Camera is always in Hi - Res in full screen

TEST(RadassControllerTest, Fullscreencamera_FT392_C20113)
{
    TestSetup S;

    S.passIteration();
    S.controller.setMode(S.cameras[1].data(), RadassMode::Low);
    S.cameras[1]->fullScreen = true;
    S.passIteration();
    ASSERT_EQ(S.cameras[1]->qualityValue, MEDIA_Quality_High);

    // This test is consistent with current RADASS implementation  and not consistent with C20113.
    // The test description is also wrong - actually we need to emulate onSlowStream() as well.
    S.cameras[1]->speed = 16;
    S.controller.onSlowStream(S.cameras[1].data());
    S.passIteration();
    S.timerFactory->passTime(1000s);
    ASSERT_EQ(S.cameras[1]->qualityValue, MEDIA_Quality_High);

    S.controller.streamBackToNormal(S.cameras[1].data());
    S.passIteration();
    ASSERT_EQ(S.cameras[1]->qualityValue, MEDIA_Quality_High);

    S.cameras[1]->fullScreen = false;
    S.passIteration();
    ASSERT_EQ(S.cameras[1]->qualityValue, MEDIA_Quality_Low);
}



} // namespace nx::vms::client::desktop::test
