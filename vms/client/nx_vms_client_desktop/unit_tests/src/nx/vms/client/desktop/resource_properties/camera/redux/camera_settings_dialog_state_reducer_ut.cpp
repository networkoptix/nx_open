#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>

#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state_reducer.h>

#include <ui/style/globals.h>

namespace nx::vms::client::desktop::test {

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

// Warning is not displayed for non-wearable cameras.
TEST_F(CameraSettingsDialogStateReducerTest, generalTabWearableMdWarningForCommonCamera)
{
    State s;
    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), true);
    ASSERT_FALSE(s.generalTabAlert.has_value());
}

// Warning is displayed when enabling MD for wearable cameras.
TEST_F(CameraSettingsDialogStateReducerTest, generalTabWearableMdWarningOnSetMd)
{
    State s = makeWearableCameraState();
    ASSERT_FALSE(s.generalTabAlert.has_value());

    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), true);
    ASSERT_TRUE(s.generalTabAlert.has_value());
    ASSERT_EQ(*s.generalTabAlert, State::GeneralTabAlert::wearableMotionDetection);

    // Check that warning is hidden when disabling MD for wearable cameras (before Apply).
    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), false);
    ASSERT_FALSE(s.generalTabAlert.has_value());

    // Warning should be visible if re-enable again.
    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), true);
    ASSERT_TRUE(s.generalTabAlert.has_value());
    ASSERT_EQ(*s.generalTabAlert, State::GeneralTabAlert::wearableMotionDetection);
}

// Warning is hidden when reloading cameras.
TEST_F(CameraSettingsDialogStateReducerTest, generalTabWearableMdWarningOnLoadCameras)
{
    State s = makeWearableCameraState();
    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), true);
    s = Reducer::loadCameras(std::move(s), {});
    ASSERT_FALSE(s.generalTabAlert.has_value());
}

// Warning is not displayed when MD is already set.
TEST_F(CameraSettingsDialogStateReducerTest, generalTabWearableMdWarningOnClearMd)
{
    State s = makeWearableCameraState();
    s.wearableMotion.enabled.setBase(true);
    ASSERT_FALSE(s.generalTabAlert.has_value());

    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), false);
    ASSERT_FALSE(s.generalTabAlert.has_value());

    // Check that warning is hidden when re-enabling MD for wearable cameras (before Apply).
    s = Reducer::setWearableMotionDetectionEnabled(std::move(s), true);
    ASSERT_FALSE(s.generalTabAlert.has_value());
}

} // namespace nx::vms::client::desktop::test
