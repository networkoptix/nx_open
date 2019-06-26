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

namespace {

static const QnUuid kSystemId1("{AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA}");
static const QnUuid kSystemId2("{BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB}");

} // namespace

namespace nx::vms::client::desktop::test {

typedef std::vector<QSharedPointer<TestCamDisplay>> CamDisplaySet;

CamDisplaySet createCamDisplaySet(int number, TestCamDisplay defaultValue = {}, int startNumber = 0)
{
    CamDisplaySet result(number);
    for (int i = 0; i < number; i++)
    {
        TestCamDisplay* camDisplay = new TestCamDisplay();
        *camDisplay = defaultValue;
        camDisplay->name = QString("Camera %1").arg(i);
        camDisplay->cameraID = QnUuid::createUuid();
        result[i].reset(camDisplay);
    }
    return result;
}

TEST(RadassControllerTest, BasicTest)
{
    auto cameras = createCamDisplaySet(kMaximumConsumersCount + 5);
    TestTimerFactory* timerFactory = new TestTimerFactory();
    TimerFactoryPtr timerFactoryPtr(timerFactory);
    RadassController controller(timerFactoryPtr);

    for (auto camera: cameras)
        controller.registerConsumer(camera.data());

    // Check that all cameras after kMaximumConsumersCount are set to LQ.
    timerFactory->passTime(kAdditionalHQRetryCounter * kTimerInterval / 2);
    for (int i = 0; i < cameras.size(); i++)
    {
        qDebug() << i;
        ASSERT_EQ(cameras[i]->qualityValue,
            i < kMaximumConsumersCount ? MEDIA_Quality_High : MEDIA_Quality_Low);
    };


}

} // namespace nx::vms::client::desktop::test
