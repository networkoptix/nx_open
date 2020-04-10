#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>

#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state_reducer.h>

#include <ui/style/globals.h>
#include <test_support/resource/camera_resource_stub.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_settings_dialog_state_conversion_functions.h>
#include <nx/core/access/access_types.h>
#include <common/common_module.h>
#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

QnScheduleTaskList makeSchedule(
    Qn::RecordingType recordingType,
    Qn::StreamQuality streamQuality = Qn::StreamQuality::normal,
    int fps = 0,
    int bitrateKbps = 0
)
{
    QnScheduleTaskList result;
    for (int day = 1; day <= 7; ++day)
    {
        result.push_back({
            /*dayOfWeek*/ day,
            /*startTime*/ 0,
            /*endTime*/ 86400,
            /*recordingType*/ recordingType,
            /*streamQuality*/ streamQuality,
            /*fps*/ fps,
            /*bitrateKbps*/ bitrateKbps
        });
    }
    return result;
}

QnScheduleTaskList makeEmptySchedule()
{
    return makeSchedule(/*recordingType*/ Qn::RecordingType::never);
}

QnScheduleTaskList makeDefaultSchedule()
{
    return makeSchedule(
        /*recordingType*/ ScheduleCellParams::kDefaultRecordingType,
        /*streamQuality*/ Qn::StreamQuality::normal,
        /*fps*/ 10
    );
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

TEST_F(CameraSettingsDialogStateReducerTest, recordingDaysBasicChecks)
{
    State s;

    // Checks basic conditions for initial (auto) state.
    ASSERT_FALSE(s.recording.maxDays.isManualMode());
    ASSERT_FALSE(s.recording.maxDays.hasManualDaysValue());
    ASSERT_TRUE(s.recording.maxDays.isApplicable());
    ASSERT_EQ(s.recording.maxDays.autoCheckState(), Qt::Checked);

    // Checks basic conditions for manual mode.
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_TRUE(s.recording.maxDays.isManualMode());
    ASSERT_TRUE(s.recording.maxDays.hasManualDaysValue());
    ASSERT_TRUE(s.recording.maxDays.isApplicable());
    ASSERT_EQ(s.recording.maxDays.autoCheckState(), Qt::Unchecked);
}

TEST_F(CameraSettingsDialogStateReducerTest, recordingDaysApplicableChecks)
{
    const QnVirtualCameraResourceList cameras = {
        CameraResourceStubPtr(new CameraResourceStub()),
        CameraResourceStubPtr(new CameraResourceStub())};

    // Same days value, same "manual" mode.
    cameras[0]->setMinDays(1);
    cameras[1]->setMinDays(1);
    State state = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(state.recording.minDays.isApplicable());

    // Same days value, different "auto" mode.
    cameras[0]->setMinDays(1);
    cameras[1]->setMinDays(-1);
    state = Reducer::loadCameras({}, cameras);
    ASSERT_FALSE(state.recording.minDays.isApplicable());


    // Same days value, same "auto" mode.
    cameras[0]->setMinDays(-1);
    cameras[1]->setMinDays(-1);
    state = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(state.recording.minDays.isApplicable());

    // Different days value, same "auto" mode.
    cameras[0]->setMinDays(-1);
    cameras[1]->setMinDays(-2);
    state = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(state.recording.minDays.isApplicable());

    // Different days value, same "manual" mode.
    cameras[0]->setMinDays(1);
    cameras[1]->setMinDays(2);
    state = Reducer::loadCameras({}, cameras);
    ASSERT_FALSE(state.recording.minDays.isApplicable());
}

TEST_F(CameraSettingsDialogStateReducerTest, fixedArchiveLengthValidation)
{
    static constexpr int kTestDaysBig = nx::vms::api::kDefaultMaxArchiveDays + 1;

    // Unchecking automatic max days when no fixed value is set: sets fixed value to default.
    State s;
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_EQ(s.recording.maxDays.days(), nx::vms::api::kDefaultMaxArchiveDays);


    // Unchecking automatic max days when fixed value is set: keeps fixed value.
    s = {};
    s.recording.maxDays = RecordingDays::maxDays(-kTestDaysBig);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_EQ(s.recording.maxDays.days(), kTestDaysBig);

    // Unchecking automatic max days when no fixed value is set: sets fixed value to default.
    // Checks that max days value stays greater or equal than fixed min days value.
    s = {};
    s.recording.minDays = RecordingDays::minDays(kTestDaysBig);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_GE(s.recording.maxDays.days(), s.recording.minDays.days());

    // Unchecking automatic max days when fixed value is set: keeps fixed value.
    // Checks that max days value stays greater or equal than fixed min days value.
    s = {};
    s.recording.maxDays = RecordingDays::maxDays(-kTestDaysBig);
    s.recording.minDays = RecordingDays::minDays(kTestDaysBig + 1);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_GE(s.recording.maxDays.days(), s.recording.minDays.days());

    // Unchecking automatic min days when no fixed value is set: sets fixed value to default.
    s = {};
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_EQ(s.recording.minDays.days(), nx::vms::api::kDefaultMinArchiveDays);

    // Unchecking automatic min days when fixed value is set: keeps fixed value.
    // Checks that min days value stays lesser or equal than fixed max days value.
    s = {};
    s.recording.minDays = RecordingDays::minDays(-kTestDaysBig);
    s.recording.maxDays = RecordingDays::maxDays(kTestDaysBig - 1);
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_LE(s.recording.minDays.days(), s.recording.maxDays.days());

    // Checking automatic min days when has fixed non-default value: keep fixed value.
    s = {};
    s.recording.minDays = RecordingDays::minDays(kTestDaysBig);
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), true);
    ASSERT_EQ(s.recording.minDays.days(), kTestDaysBig);

    // Setting fixed min days greater than fixed max days pushes max days up.
    s = {};
    s.recording.minDays.setManualMode();
    s.recording.maxDays = RecordingDays::maxDays(kTestDaysBig - 1);
    s = Reducer::setMinRecordingDaysValue(std::move(s), kTestDaysBig);
    ASSERT_EQ(s.recording.minDays.days(), kTestDaysBig);
    ASSERT_LE(s.recording.minDays.days(), s.recording.maxDays.days());
}

// Setup fixed value of recording min days, leaving max days as auto.
TEST_F(CameraSettingsDialogStateReducerTest, setFixedRecordingMinDays)
{
    static constexpr int kDefaultDays = -nx::vms::api::kDefaultMaxArchiveDays;
    static constexpr int kTestDaysBig = nx::vms::api::kDefaultMaxArchiveDays + 1;

    CameraResourceStubPtr camera(new CameraResourceStub());

    // Unchecking 'auto' must keep previously saved (as negative) value.
    camera->setMinDays(-kTestDaysBig);

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_TRUE(initial.recording.minDays.autoCheckState() == Qt::Checked);
    ASSERT_FALSE(initial.recording.minDays.isManualMode());
    ASSERT_FALSE(initial.recording.minDays.hasManualDaysValue());

    State unchecked = Reducer::setMinRecordingDaysAutomatic(std::move(initial), false);
    ASSERT_FALSE(initial.recording.minDays.isManualMode());
    ASSERT_FALSE(initial.recording.minDays.hasManualDaysValue());
    ASSERT_EQ(unchecked.recording.minDays.days(), kTestDaysBig);

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(unchecked, {camera});
    State reloaded = Reducer::loadCameras(std::move(unchecked), {camera});
    ASSERT_EQ(reloaded.recording.minDays.days(), kTestDaysBig);
}

// Schedule brush should be correctly initialized after loadCameras.
TEST_F(CameraSettingsDialogStateReducerTest, scheduleBrushIsValidAfterLoadCameras)
{
    static constexpr int kDefaultFps = 30;

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setMaxFps(kDefaultFps);

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(initial.devicesDescription.maxFps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.fps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.recordingType, Qn::RecordingType::always);
    ASSERT_GT(initial.recording.minBitrateMbps, 0);
    ASSERT_GT(initial.recording.maxBitrateMpbs, initial.recording.minBitrateMbps);
}

// If clean schedule, fps brush should be reset to a default value.
TEST_F(CameraSettingsDialogStateReducerTest, brushFpsIsValidAfterCleanSchedule)
{
    static constexpr int kDefaultFps = 30;

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setMaxFps(kDefaultFps);
    const QnVirtualCameraResourceList cameras{camera};

    State initial = Reducer::loadCameras({}, cameras);

    State beforeClean = Reducer::setScheduleBrushRecordingType(std::move(initial),
        Qn::RecordingType::never);
    ASSERT_EQ(beforeClean.recording.brush.fps, kDefaultFps);

    State afterClean = Reducer::setSchedule(std::move(beforeClean), makeEmptySchedule());
    ASSERT_EQ(afterClean.recording.brush.fps, kDefaultFps);

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(afterClean, cameras);
    State reloaded = Reducer::loadCameras(std::move(afterClean), cameras);
    ASSERT_EQ(reloaded.recording.brush.fps, kDefaultFps);
}

// If clean schedule, fps brush should be reset to a default value.
TEST_F(CameraSettingsDialogStateReducerTest, brushQualityIsLoaded)
{
    CameraResourceStubPtr camera(new CameraResourceStub());
    const QnVirtualCameraResourceList cameras{ camera };

    static constexpr auto kCustomQuality = Qn::StreamQuality::low;

    State initial = Reducer::loadCameras({}, cameras);

    State afterSetup = Reducer::setSchedule(std::move(initial),
        makeSchedule(Qn::RecordingType::always, kCustomQuality));

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(afterSetup, cameras);
    State reloaded = Reducer::loadCameras(std::move(afterSetup), cameras);
    ASSERT_EQ(reloaded.recording.brush.quality, kCustomQuality);
}

// Schedule brush correctness when bitrate switches from custom to predefined and back.
TEST_F(CameraSettingsDialogStateReducerTest, brushBitrateIsValidOnScheduleBrushChange)
{
    static constexpr int kDefaultFps = 30;

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setMaxFps(kDefaultFps);

    State initial = Reducer::toggleCustomBitrateVisible(Reducer::loadCameras({}, {camera}));
    ASSERT_TRUE(initial.recording.customBitrateAvailable && initial.recording.customBitrateVisible);

    State high = Reducer::setScheduleBrushQuality(std::move(initial), Qn::StreamQuality::high);
    const auto highBrush = high.recording.brush;
    const auto highBitrate = high.recording.bitrateMbps;
    ASSERT_TRUE(highBrush.isAutomaticBitrate());

    State medium = Reducer::setScheduleBrushQuality(std::move(high), Qn::StreamQuality::normal);
    const auto mediumBrush = medium.recording.brush;
    const auto mediumBitrate = medium.recording.bitrateMbps;
    ASSERT_TRUE(mediumBrush.isAutomaticBitrate());

    const auto customBitrate = (mediumBitrate + highBitrate) * 0.5;
    State custom = Reducer::setRecordingBitrateMbps(std::move(medium), customBitrate);
    const auto customBrush = custom.recording.brush;
    ASSERT_FALSE(customBrush.isAutomaticBitrate());
    ASSERT_EQ(customBitrate, custom.recording.bitrateMbps);
    ASSERT_EQ(customBitrate, customBrush.bitrateMbps);

    State highAfterCustom = Reducer::setScheduleBrush(std::move(custom), highBrush);
    ASSERT_EQ(highBrush, highAfterCustom.recording.brush);
    ASSERT_EQ(highBitrate, highAfterCustom.recording.bitrateMbps);

    State customAfterHigh = Reducer::setScheduleBrush(std::move(highAfterCustom), customBrush);
    ASSERT_EQ(customBrush, customAfterHigh.recording.brush);
    ASSERT_EQ(customBitrate, customBrush.bitrateMbps);
    ASSERT_EQ(customBitrate, customAfterHigh.recording.bitrateMbps);

    State mediumAfterCustom = Reducer::setScheduleBrush(std::move(customAfterHigh), mediumBrush);
    ASSERT_EQ(mediumBrush, mediumAfterCustom.recording.brush);
    ASSERT_EQ(mediumBitrate, mediumAfterCustom.recording.bitrateMbps);
}

// When a user enables recording with an empty schedule, special notification should appear.
TEST_F(CameraSettingsDialogStateReducerTest, recordingAlertIfScheduleIsEmpty)
{
    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setScheduleTasks(makeEmptySchedule());

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_FALSE(initial.recording.enabled());
    ASSERT_FALSE(initial.recordingHint.has_value());

    const auto makeRecordingState =
        [camera]
        {
            return Reducer::setRecordingEnabled(Reducer::loadCameras({}, {camera}), true);
        };

    auto recordingEnabled = makeRecordingState();
    ASSERT_TRUE(recordingEnabled.recording.enabled());
    ASSERT_TRUE(recordingEnabled.recordingHint.has_value());
    ASSERT_EQ(*recordingEnabled.recordingHint, State::RecordingHint::emptySchedule);

    // Warning should be hidden if we disable recording.
    State recordingDisabled = Reducer::setRecordingEnabled(makeRecordingState(), false);
    ASSERT_FALSE(recordingDisabled.recordingHint.has_value());

    // Warning should be hidden if we setup some schedule.
    State scheduleSet = Reducer::setSchedule(makeRecordingState(), makeDefaultSchedule());
    ASSERT_FALSE(recordingDisabled.recordingHint.has_value());
}

// "Motion" and "Motion + Lo-Res" recording types must not be available if secondary stream is
// disabled on the expert settings page.
TEST_F(CameraSettingsDialogStateReducerTest, disableMotionBrushIfDualStreamingIsDisabled)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setHasDualStreaming(true);

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    ASSERT_EQ(initial.devicesDescription.supportsMotionStreamOverride, CombinedValue::All);
    ASSERT_TRUE(initial.isMotionDetectionEnabled());
    ASSERT_TRUE(initial.isDualStreamingEnabled());

    State setMotionAndLowBrush = Reducer::setScheduleBrushRecordingType(
        std::move(initial), Qn::RecordingType::motionAndLow);
    ASSERT_EQ(setMotionAndLowBrush.recording.brush.recordingType, Qn::RecordingType::motionAndLow);

    State forcedPrimaryMotionStream = Reducer::setForcedMotionStreamType(
        std::move(setMotionAndLowBrush), nx::vms::api::StreamIndex::primary);
    ASSERT_TRUE(initial.isMotionDetectionEnabled());
    ASSERT_TRUE(initial.isDualStreamingEnabled());
    ASSERT_EQ(setMotionAndLowBrush.recording.brush.recordingType, Qn::RecordingType::motionAndLow);

    State disabledDualStreaming = Reducer::setDualStreamingDisabled(
        std::move(forcedPrimaryMotionStream), true);
    ASSERT_TRUE(disabledDualStreaming.isMotionDetectionEnabled());
    ASSERT_FALSE(disabledDualStreaming.isDualStreamingEnabled());
    ASSERT_EQ(disabledDualStreaming.recording.brush.recordingType, Qn::RecordingType::always);

    State setMotionOnlyBrush = Reducer::setScheduleBrushRecordingType(
        std::move(disabledDualStreaming), Qn::RecordingType::motionOnly);
    ASSERT_EQ(setMotionOnlyBrush.recording.brush.recordingType, Qn::RecordingType::motionOnly);

    State forcedNoMotionStream = Reducer::setForcedMotionStreamType(
        std::move(setMotionOnlyBrush), nx::vms::api::StreamIndex::undefined);
    ASSERT_FALSE(forcedNoMotionStream.isMotionDetectionEnabled());
    ASSERT_FALSE(forcedNoMotionStream.isDualStreamingEnabled());
    ASSERT_EQ(forcedNoMotionStream.recording.brush.recordingType, Qn::RecordingType::always);

    State enabledDualStreaming = Reducer::setDualStreamingDisabled(
        std::move(forcedNoMotionStream), false);
    ASSERT_TRUE(enabledDualStreaming.isMotionDetectionEnabled());
    ASSERT_TRUE(enabledDualStreaming.isDualStreamingEnabled());
}

// "Motion" and "Motion + Lo-Res" recording type must always be available if camera settings
// optimization is off.
TEST_F(CameraSettingsDialogStateReducerTest, enableMotionBrushIfDeviceOptimizationIsDisabled)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setHasDualStreaming(true);

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), { camera });
    ASSERT_TRUE(initial.isMotionDetectionEnabled());
    ASSERT_TRUE(initial.isDualStreamingEnabled());

    State disabledDualStreaming = Reducer::setDualStreamingDisabled(std::move(initial), true);
    ASSERT_FALSE(disabledDualStreaming.isMotionDetectionEnabled());
    ASSERT_FALSE(disabledDualStreaming.isDualStreamingEnabled());

    State disableDeviceOptimization = Reducer::setSettingsOptimizationEnabled(
        std::move(disabledDualStreaming), false);

    ASSERT_TRUE(disableDeviceOptimization.isMotionDetectionEnabled());
    ASSERT_TRUE(disableDeviceOptimization.isDualStreamingEnabled());
}

// "Motion" recording type must be available if user forces MD on the primary stream.
TEST_F(CameraSettingsDialogStateReducerTest, disableMotionIfSecondaryStreamDisabled)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setHasDualStreaming(true);

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), { camera });
    ASSERT_EQ(initial.devicesDescription.supportsMotionStreamOverride, CombinedValue::All);

    State disabledDualStreaming = Reducer::setDualStreamingDisabled(std::move(initial), true);
    ASSERT_FALSE(disabledDualStreaming.isMotionDetectionEnabled());

    State forcedMdOnPrimaryStream = Reducer::setForcedMotionStreamType(
        std::move(disabledDualStreaming), nx::vms::api::StreamIndex::primary);
    ASSERT_TRUE(forcedMdOnPrimaryStream.isMotionDetectionEnabled());
}

// Validate VMS-17723 scenario: each camera schedule is OK, but reducer suggests to disable motion.
TEST_F(CameraSettingsDialogStateReducerTest, DISABLED_skipMotionAlertIfScheduleIsNotToBeModified)
{
    // Given camera without motion support and with valid schedule.
    CameraResourceStubPtr cameraWithNoMotion(new CameraResourceStub());
    cameraWithNoMotion->setDisableDualStreaming(true);
    cameraWithNoMotion->setScheduleTasks(makeSchedule(Qn::RecordingType::always));
    EXPECT_EQ(Reducer::loadCameras({}, {cameraWithNoMotion}).scheduleAlert, std::nullopt);

    // Given camera with motion support and with valid motion-only schedule.
    CameraResourceStubPtr cameraWithMotion(new CameraResourceStub());
    cameraWithMotion->setScheduleTasks(makeSchedule(Qn::RecordingType::motionOnly));
    EXPECT_EQ(Reducer::loadCameras({}, {cameraWithMotion}).scheduleAlert, std::nullopt);

    // Selecting both cameras must not emit any schedule alerts.
    const auto initial = Reducer::loadCameras({}, {cameraWithNoMotion, cameraWithMotion});
    ASSERT_EQ(initial.scheduleAlert, std::nullopt);
}

// "wearableIgnoreTimeZone" property test
TEST_F(CameraSettingsDialogStateReducerTest, wearableIgnoreTimeZone)
{
    // Test default state value
    State state1 = Reducer::setSettingsOptimizationEnabled({}, true);
    ASSERT_FALSE(state1.wearableIgnoreTimeZone);
    state1 = Reducer::setWearableIgnoreTimeZone(std::move(state1), true);
    ASSERT_TRUE(state1.wearableIgnoreTimeZone);

    // Test default camera state
    CameraResourceStubPtr camera(new CameraResourceStub());
    ASSERT_FALSE(camera->wearableIgnoreTimeZone());
    State state2 = Reducer::loadCameras({}, { camera });
    ASSERT_FALSE(state2.wearableIgnoreTimeZone);

    // Test value ignoring for non wearable cameras
    ASSERT_EQ(state2.devicesDescription.isWearable, CombinedValue::None);
    state2 = Reducer::setWearableIgnoreTimeZone(std::move(state2), true);
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state2, {camera});
    ASSERT_FALSE(camera->wearableIgnoreTimeZone());

    // Test value setting for wearable cameras
    camera->addFlags(Qn::wearable_camera);
    State state3 = Reducer::loadCameras({}, { camera });
    ASSERT_EQ(state3.devicesDescription.isWearable, CombinedValue::All);
    state3 = Reducer::setWearableIgnoreTimeZone(std::move(state3), true);
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state3, {camera});
    ASSERT_TRUE(camera->wearableIgnoreTimeZone());
}

} // namespace test
} // namespace nx::vms::client::desktop
