#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>

#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state_reducer.h>

#include <ui/style/globals.h>
#include <test_support/resource/camera_resource_stub.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_settings_dialog_state_conversion_functions.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

QnScheduleTaskList makeEmptySchedule()
{
    QnScheduleTaskList result;
    for (int day = 1; day <= 7; ++day)
    {
        result.push_back({
            /*dayOfWeek*/ day,
            /*startTime*/ 0,
            /*endTime*/ 86400,
            /*recordingType*/ Qn::RecordingType::never,
            /*streamQuality*/ Qn::StreamQuality::highest,
            /*fps*/ 0,
            /*bitrateKbps*/ 0
		});
    }
    return result;
}

} // namespace

class CameraSettingsDialogStateReducerTest: public testing::Test
{
public:
    using State = CameraSettingsDialogState;
    using Reducer = CameraSettingsDialogStateReducer;

protected:
    static State makeWearableCameraState()
    {
        State s;
        s.devicesCount = 1;
        s.deviceType = QnCameraDeviceType::Camera;
        s.devicesDescription.isWearable = CombinedValue::All;
        s.readOnly = false;
        return s;
    }
};

TEST_F(CameraSettingsDialogStateReducerTest, fixedArchiveLengthValidation)
{
    static constexpr int kTestDaysBig = nx::vms::api::kDefaultMaxArchiveDays + 1;

    // Unchecking automatic max days when no fixed value is set: sets fixed value to default.
    State s;
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.maxDays.value(), nx::vms::api::kDefaultMaxArchiveDays);

    // Unchecking automatic max days when fixed value is set: keeps fixed value.
    s = {};
    s.recording.maxDays.value.setBase(kTestDaysBig);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.maxDays.value(), kTestDaysBig);

    // Unchecking automatic max days when no fixed value is set: sets fixed value to default.
    // Checks that max days value stays greater or equal than fixed min days value.
    s = {};
    s.recording.minDays.automatic.setBase(false);
    s.recording.minDays.value.setBase(kTestDaysBig);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_GE(s.recording.maxDays.value(), s.recording.minDays.value());

    // Unchecking automatic max days when fixed value is set: keeps fixed value.
    // Checks that max days value stays greater or equal than fixed min days value.
    s = {};
    s.recording.maxDays.value.setBase(kTestDaysBig);
    s.recording.minDays.automatic.setBase(false);
    s.recording.minDays.value.setBase(kTestDaysBig + 1);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_GE(s.recording.maxDays.value(), s.recording.minDays.value());

    // Unchecking automatic min days when no fixed value is set: sets fixed value to default.
    s = {};
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.minDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.minDays.value(), nx::vms::api::kDefaultMinArchiveDays);

    // Unchecking automatic min days when fixed value is set: keeps fixed value.
    s = {};
    s.recording.minDays.value.setBase(kTestDaysBig);
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.minDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.minDays.value(), kTestDaysBig);

    // Unchecking automatic min days when fixed value is set: keeps fixed value.
    // Checks that min days value stays lesser or equal than fixed max days value.
    s = {};
    s.recording.minDays.value.setBase(kTestDaysBig);
    s.recording.maxDays.automatic.setBase(false);
    s.recording.maxDays.value.setBase(kTestDaysBig - 1);
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.minDays.automatic.valueOr(true));
    ASSERT_LE(s.recording.minDays.value(), s.recording.maxDays.value());

    // Setting fixed max days less than fixed min days pushes min days down.
    s = {};
    s.recording.minDays.automatic.setBase(false);
    s.recording.maxDays.automatic.setBase(false);
    s.recording.minDays.value.setBase(kTestDaysBig + 1);
    s = Reducer::setMaxRecordingDaysValue(std::move(s), kTestDaysBig);
    ASSERT_EQ(s.recording.maxDays.value(), kTestDaysBig);
    ASSERT_GE(s.recording.maxDays.value(), s.recording.minDays.value());

    // Setting fixed min days greater than fixed max days pushes max days up.
    s = {};
    s.recording.minDays.automatic.setBase(false);
    s.recording.maxDays.automatic.setBase(false);
    s.recording.maxDays.value.setBase(kTestDaysBig - 1);
    s = Reducer::setMinRecordingDaysValue(std::move(s), kTestDaysBig);
    ASSERT_EQ(s.recording.minDays.value(), kTestDaysBig);
    ASSERT_LE(s.recording.minDays.value(), s.recording.maxDays.value());
}

// If clean schedule, fps brush should be reset to a default value.
TEST_F(CameraSettingsDialogStateReducerTest, scheduleBrushAfterCleanSchedule)
{
    static constexpr int kDefaultFps = 30;

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setMaxFps(kDefaultFps);
    const QnVirtualCameraResourceList cameras{camera};

    State initial = Reducer::loadCameras({}, cameras);
    ASSERT_EQ(initial.devicesDescription.maxFps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.fps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.recordingType, Qn::RecordingType::always);

    State beforeClean = Reducer::setScheduleBrushRecordingType(std::move(initial),
		Qn::RecordingType::never);
    ASSERT_EQ(beforeClean.recording.brush.fps, kDefaultFps);

    State afterClean = Reducer::setSchedule(std::move(beforeClean), makeEmptySchedule());
    ASSERT_EQ(afterClean.recording.brush.fps, kDefaultFps);

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(afterClean, cameras);
    State reloaded = Reducer::loadCameras(std::move(afterClean), cameras);
    ASSERT_EQ(reloaded.recording.brush.fps, kDefaultFps);
}

} // namespace test
} // namespace nx::vms::client::desktop
