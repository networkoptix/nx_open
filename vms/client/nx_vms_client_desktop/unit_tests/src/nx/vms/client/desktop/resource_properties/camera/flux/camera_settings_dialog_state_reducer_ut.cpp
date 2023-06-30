// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <core/resource/layout_resource.h>
#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_state_reducer.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_settings_dialog_state_conversion_functions.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/private/motion_stream_alerts.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {
namespace test {

namespace {

static constexpr QSize kTooBigResolution(
    QnVirtualCameraResource::kMaximumMotionDetectionPixels + 1,
    QnVirtualCameraResource::kMaximumMotionDetectionPixels + 1);

QnScheduleTaskList makeSchedule(
    Qn::RecordingType recordingType,
    Qn::RecordingMetadataTypes metadataTypes,
    Qn::StreamQuality streamQuality = Qn::StreamQuality::normal,
    int fps = 0,
    int bitrateKbps = 0
)
{
    QnScheduleTaskList result;
    for (qint8 day = 1; day <= 7; ++day)
    {
        result.push_back({
            /*startTime*/ 0,
            /*endTime*/ seconds(24h).count(),
            /*dayOfWeek*/ day,
            /*recordingType*/ recordingType,
            /*streamQuality*/ streamQuality,
            /*fps*/ fps,
            /*bitrateKbps*/ bitrateKbps,
            metadataTypes
        });
    }
    return result;
}

QnScheduleTaskList makeEmptySchedule()
{
    return makeSchedule(
        /*recordingType*/ Qn::RecordingType::never,
        /*metadataTypes*/ Qn::RecordingMetadataTypes());
}

QnScheduleTaskList makeDefaultSchedule()
{
    return makeSchedule(
        /*recordingType*/ Qn::RecordingType::always,
        /*metadataTypes*/ Qn::RecordingMetadataTypes(),
        /*streamQuality*/ Qn::StreamQuality::normal,
        /*fps*/ 10
    );
}

void ensureScheduleFps(const CameraSettingsDialogState& state, int fps)
{
    for (const auto& task: state.recording.schedule())
        ASSERT_EQ(task.fps, fps);
}

core::AnalyticsEngineInfo makeAnalyticsEngine(bool isLicenseRequired)
{
    core::AnalyticsEngineInfo engineInfo;
    engineInfo.id = QnUuid::createUuid();
    engineInfo.isLicenseRequired = isLicenseRequired;
    return engineInfo;
};

api::LicenseSummaryDataEx makeLicenseSummary()
{
    api::LicenseSummaryDataEx licenseSummary;
    licenseSummary.total = 10;
    licenseSummary.available = 10;
    licenseSummary.inUse = 5;
    return licenseSummary;
};

} // namespace

class CameraSettingsDialogStateReducerTest: public ContextBasedTest
{
public:
    using State = CameraSettingsDialogState;
    using Reducer = CameraSettingsDialogStateReducer;

protected:
    CameraResourceStubPtr createCamera() const
    {
        const auto resourcePool = systemContext()->resourcePool();
        const auto camera = CameraResourceStubPtr(new CameraResourceStub());
        resourcePool->addResource(camera);
        return camera;
    }

    CameraResourceStubPtr createNvrCamera() const
    {
        const auto resourcePool = systemContext()->resourcePool();
        const auto camera = CameraResourceStubPtr(new CameraResourceStub());
        camera->markCameraAsNvr();
        resourcePool->addResource(camera);
        return camera;
    }

    static State makeVirtualCameraState()
    {
        State s;
        s.devicesCount = 1;
        s.deviceType = QnCameraDeviceType::Camera;
        s.devicesDescription.isVirtualCamera = CombinedValue::All;
        s.readOnly = false;
        return s;
    }
};

TEST_F(CameraSettingsDialogStateReducerTest, recordingPeriodsBasicChecks)
{
    State s;

    // Checks basic conditions for initial (auto) state.
    ASSERT_FALSE(s.recording.maxPeriod.isManualMode());
    ASSERT_FALSE(s.recording.maxPeriod.hasManualPeriodValue());
    ASSERT_TRUE(s.recording.maxPeriod.isApplicable());
    ASSERT_EQ(s.recording.maxPeriod.autoCheckState(), Qt::Checked);

    // Checks basic conditions for manual mode.
    s = Reducer::setMaxRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_TRUE(s.recording.maxPeriod.isManualMode());
    ASSERT_TRUE(s.recording.maxPeriod.hasManualPeriodValue());
    ASSERT_TRUE(s.recording.maxPeriod.isApplicable());
    ASSERT_EQ(s.recording.maxPeriod.autoCheckState(), Qt::Unchecked);
}

TEST_F(CameraSettingsDialogStateReducerTest, recordingPeriodsApplicableChecks)
{
    const QnVirtualCameraResourceList cameras = {createCamera(), createCamera()};

    // Same recording periods values, same "manual" mode.
    cameras[0]->setMinPeriod(std::chrono::days(1));
    cameras[1]->setMinPeriod(std::chrono::days(1));
    State state = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(state.recording.minPeriod.isApplicable());

    // Same recording periods values, different "auto" mode.
    cameras[0]->setMinPeriod(std::chrono::days(1));
    cameras[1]->setMinPeriod(std::chrono::days(-1));
    state = Reducer::loadCameras({}, cameras);
    ASSERT_FALSE(state.recording.minPeriod.isApplicable());


    // Same recording periods values, same "auto" mode.
    cameras[0]->setMinPeriod(std::chrono::days(-1));
    cameras[1]->setMinPeriod(std::chrono::days(-1));
    state = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(state.recording.minPeriod.isApplicable());

    // Different recording periods values, same "auto" mode.
    cameras[0]->setMinPeriod(std::chrono::days(-1));
    cameras[1]->setMinPeriod(std::chrono::days(-2));
    state = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(state.recording.minPeriod.isApplicable());

    // Different recording periods values, same "manual" mode.
    cameras[0]->setMinPeriod(std::chrono::days(1));
    cameras[1]->setMinPeriod(std::chrono::days(2));
    state = Reducer::loadCameras({}, cameras);
    ASSERT_FALSE(state.recording.minPeriod.isApplicable());
}

TEST_F(CameraSettingsDialogStateReducerTest, fixedArchiveLengthValidation)
{
    static constexpr auto kTestBigPeriod =
        nx::vms::api::kDefaultMaxArchivePeriod + std::chrono::days(1);

    // Unchecking automatic max period when no fixed value is set: sets fixed value to default.
    State s;
    s = Reducer::setMaxRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_EQ(s.recording.maxPeriod.displayValue(), nx::vms::api::kDefaultMaxArchivePeriod);

    // Unchecking automatic max period when fixed value is set: keeps fixed value.
    s = {};
    s.recording.maxPeriod = RecordingPeriod::maxPeriod(-kTestBigPeriod);
    s = Reducer::setMaxRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_EQ(s.recording.maxPeriod.displayValue(), kTestBigPeriod);

    // Unchecking automatic max period when no fixed value is set: sets fixed value to default.
    // Checks that max period value stays greater or equal than fixed min period value.
    s = {};
    s.recording.minPeriod = RecordingPeriod::minPeriod(kTestBigPeriod);
    s = Reducer::setMaxRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_GE(s.recording.maxPeriod.displayValue(), s.recording.minPeriod.displayValue());

    // Unchecking automatic max period when fixed value is set: keeps fixed value.
    // Checks that max period value stays greater or equal than fixed min period value.
    s = {};
    s.recording.maxPeriod = RecordingPeriod::maxPeriod(-kTestBigPeriod);
    s.recording.minPeriod = RecordingPeriod::minPeriod(kTestBigPeriod + std::chrono::days(1));
    s = Reducer::setMaxRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_GE(s.recording.maxPeriod.displayValue(), s.recording.minPeriod.displayValue());

    // Unchecking automatic min period when no fixed value is set: sets fixed value to default.
    s = {};
    s = Reducer::setMinRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_EQ(s.recording.minPeriod.displayValue(), nx::vms::api::kDefaultMinArchivePeriod);

    // Unchecking automatic min period when fixed value is set: keeps fixed value.
    // Checks that min period value stays lesser or equal than fixed max period value.
    s = {};
    s.recording.minPeriod = RecordingPeriod::minPeriod(-kTestBigPeriod);
    s.recording.maxPeriod = RecordingPeriod::maxPeriod(kTestBigPeriod - std::chrono::days(1));
    s = Reducer::setMinRecordingPeriodAutomatic(std::move(s), false);
    ASSERT_LE(s.recording.minPeriod.displayValue(), s.recording.maxPeriod.displayValue());

    // Checking automatic min period when has fixed non-default value: keep fixed value.
    s = {};
    s.recording.minPeriod = RecordingPeriod::minPeriod(kTestBigPeriod);
    s = Reducer::setMinRecordingPeriodAutomatic(std::move(s), true);
    ASSERT_EQ(s.recording.minPeriod.displayValue(), kTestBigPeriod);

    // Setting fixed min period greater than fixed max period pushes max period up.
    s = {};
    s.recording.minPeriod.setManualMode();
    s.recording.maxPeriod = RecordingPeriod::maxPeriod(kTestBigPeriod - std::chrono::days(1));
    s = Reducer::setMinRecordingPeriodValue(std::move(s), kTestBigPeriod);
    ASSERT_EQ(s.recording.minPeriod.displayValue(), kTestBigPeriod);
    ASSERT_LE(s.recording.minPeriod.displayValue(), s.recording.maxPeriod.displayValue());
}

// Setup fixed value of recording min period, leaving max period as auto.
TEST_F(CameraSettingsDialogStateReducerTest, setFixedRecordingMinPeriod)
{
    static constexpr auto kTestBigPeriod =
        nx::vms::api::kDefaultMaxArchivePeriod + std::chrono::days(1);

    const auto camera = createCamera();

    // Unchecking 'auto' must keep previously saved (as negative) value.
    camera->setMinPeriod(std::chrono::days(-kTestBigPeriod));

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_TRUE(initial.recording.minPeriod.autoCheckState() == Qt::Checked);
    ASSERT_FALSE(initial.recording.minPeriod.isManualMode());
    ASSERT_FALSE(initial.recording.minPeriod.hasManualPeriodValue());

    State unchecked = Reducer::setMinRecordingPeriodAutomatic(std::move(initial), false);
    ASSERT_FALSE(initial.recording.minPeriod.isManualMode());
    ASSERT_FALSE(initial.recording.minPeriod.hasManualPeriodValue());
    ASSERT_EQ(unchecked.recording.minPeriod.displayValue(), kTestBigPeriod);

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(unchecked, {camera});
    State reloaded = Reducer::loadCameras(std::move(unchecked), {camera});
    ASSERT_EQ(reloaded.recording.minPeriod.displayValue(), kTestBigPeriod);
}

// Schedule brush should be correctly initialized after loadCameras.
TEST_F(CameraSettingsDialogStateReducerTest, motionScheduleBrushIsValidAfterLoadCameras)
{
    static constexpr int kDefaultFps = 30;

    const auto camera = createCamera();
    camera->setMaxFps(kDefaultFps);

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(initial.devicesDescription.maxFps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.fps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.recordingType, Qn::RecordingType::always);
    ASSERT_EQ(initial.recording.brush.metadataTypes, {});
    ASSERT_GT(initial.recording.minBitrateMbps, 0);
    ASSERT_GT(initial.recording.maxBitrateMpbs, initial.recording.minBitrateMbps);
}

// Schedule brush should be correctly initialized after loadCameras.
TEST_F(CameraSettingsDialogStateReducerTest, alwaysScheduleBrushIsValidAfterLoadCameras)
{
    static constexpr int kDefaultFps = 30;

    const auto camera = createCamera();
    camera->setMaxFps(kDefaultFps);
    camera->setMotionType(nx::vms::api::MotionType::none);
    camera->setProperty(ResourcePropertyKey::kSupportedMotion, QString());

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(initial.devicesDescription.maxFps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.fps, kDefaultFps);
    ASSERT_EQ(initial.recording.brush.recordingType, Qn::RecordingType::always);
    ASSERT_EQ(initial.recording.brush.metadataTypes, Qn::RecordingMetadataTypes());
    ASSERT_GT(initial.recording.minBitrateMbps, 0);
    ASSERT_GT(initial.recording.maxBitrateMpbs, initial.recording.minBitrateMbps);
}

// If clean schedule, fps brush should be reset to a default value.
TEST_F(CameraSettingsDialogStateReducerTest, brushFpsIsValidAfterCleanSchedule)
{
    static constexpr int kDefaultFps = 30;

    const auto camera = createCamera();
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

// Should update schedule and brush FPS after camera max FPS decrease.
TEST_F(CameraSettingsDialogStateReducerTest, scheduleFpsIsValidAfterCameraUpdate)
{
    static constexpr int k60Fps = 60;
    static constexpr int k120Fps = 120;
    static constexpr int k30Fps = 30;

    const auto camera = createCamera();
    camera->setStatus(nx::vms::api::ResourceStatus::online);
    camera->setMaxFps(k60Fps);
    QnVirtualCameraResourceList cameras{camera};
    ASSERT_TRUE(camera->getScheduleTasks().empty());

    State first = Reducer::loadCameras({}, cameras);
    ASSERT_FALSE(first.recording.schedule().empty());
    for (const auto& task : first.recording.schedule())
    {
        ASSERT_EQ(task.fps, k60Fps);
    }
    ASSERT_EQ(first.recording.brush.fps, k60Fps);

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(first, cameras);
    ASSERT_FALSE(camera->getScheduleTasks().empty());

    camera->setMaxFps(k120Fps);
    State second = Reducer::loadCameras({}, cameras);
    EXPECT_FALSE(second.recording.schedule().empty());
    for (const auto& task: second.recording.schedule())
    {
        EXPECT_EQ(task.fps, k60Fps);
    }
    EXPECT_EQ(second.recording.brush.fps, k60Fps);

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(second, cameras);
    ASSERT_FALSE(camera->getScheduleTasks().empty());

    camera->setMaxFps(k30Fps);
    State third = Reducer::loadCameras({}, cameras);
    EXPECT_FALSE(third.recording.schedule().empty());
    for (const auto& task: third.recording.schedule())
    {
        EXPECT_EQ(task.fps, k30Fps);
    }
    EXPECT_EQ(third.recording.brush.fps, k30Fps);
}

TEST_F(CameraSettingsDialogStateReducerTest, maxInitialScheduleFpsForUnauthorized)
{
    const auto camera1 = createCamera();
    camera1->setMaxFps(60);
    camera1->setStatus(nx::vms::api::ResourceStatus::unauthorized);

    State singleCamera = Reducer::loadCameras({}, {camera1});

    static constexpr int kUnauthorizedFps = 15;
    ASSERT_EQ(singleCamera.maxRecordingBrushFps(), kUnauthorizedFps);
    ensureScheduleFps(singleCamera, kUnauthorizedFps);

    const auto camera2 = createCamera();
    camera2->setMaxFps(60);
    camera2->setStatus(nx::vms::api::ResourceStatus::unauthorized);

    State multipleCameras = Reducer::loadCameras({}, {camera1, camera2});
    ASSERT_EQ(multipleCameras.maxRecordingBrushFps(), kUnauthorizedFps);
    ensureScheduleFps(multipleCameras, kUnauthorizedFps);
}

// If clean schedule, fps brush should be reset to a default value.
TEST_F(CameraSettingsDialogStateReducerTest, brushQualityIsLoaded)
{
    const QnVirtualCameraResourceList cameras{createCamera()};

    static constexpr auto kCustomQuality = Qn::StreamQuality::low;

    State initial = Reducer::loadCameras({}, cameras);

    State afterSetup = Reducer::setSchedule(std::move(initial),
        makeSchedule(Qn::RecordingType::always, Qn::RecordingMetadataTypes(), kCustomQuality));

    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(afterSetup, cameras);
    State reloaded = Reducer::loadCameras(std::move(afterSetup), cameras);
    ASSERT_EQ(reloaded.recording.brush.streamQuality, kCustomQuality);
}

// Schedule brush correctness when bitrate switches from custom to predefined and back.
TEST_F(CameraSettingsDialogStateReducerTest, brushBitrateIsValidOnScheduleBrushChange)
{
    static constexpr int kDefaultFps = 30;

    const auto camera = createCamera();
    camera->setMaxFps(kDefaultFps);

    State initial = Reducer::toggleCustomBitrateVisible(Reducer::loadCameras({}, {camera}));
    ASSERT_TRUE(initial.recording.customBitrateAvailable && initial.recording.customBitrateVisible);

    State high = Reducer::setScheduleBrushQuality(std::move(initial), Qn::StreamQuality::high);
    const auto highBrush = high.recording.brush;
    const auto highBitrate = high.recording.bitrateMbps;
    ASSERT_TRUE(highBrush.isAutoBitrate());

    State medium = Reducer::setScheduleBrushQuality(std::move(high), Qn::StreamQuality::normal);
    const auto mediumBrush = medium.recording.brush;
    const auto mediumBitrate = medium.recording.bitrateMbps;
    ASSERT_TRUE(mediumBrush.isAutoBitrate());

    const auto customBitrate = (mediumBitrate + highBitrate) * 0.5f;
    State custom = Reducer::setRecordingBitrateMbps(std::move(medium), customBitrate);
    const auto customBrush = custom.recording.brush;
    ASSERT_FALSE(customBrush.isAutoBitrate());
    ASSERT_EQ(customBitrate, custom.recording.bitrateMbps);
    ASSERT_EQ(customBitrate, customBrush.bitrateMbitPerSec);

    State highAfterCustom = Reducer::setScheduleBrush(std::move(custom), highBrush);
    ASSERT_EQ(highBrush, highAfterCustom.recording.brush);
    ASSERT_EQ(highBitrate, highAfterCustom.recording.bitrateMbps);

    State customAfterHigh = Reducer::setScheduleBrush(std::move(highAfterCustom), customBrush);
    ASSERT_EQ(customBrush, customAfterHigh.recording.brush);
    ASSERT_EQ(customBitrate, customBrush.bitrateMbitPerSec);
    ASSERT_EQ(customBitrate, customAfterHigh.recording.bitrateMbps);

    State mediumAfterCustom = Reducer::setScheduleBrush(std::move(customAfterHigh), mediumBrush);
    ASSERT_EQ(mediumBrush, mediumAfterCustom.recording.brush);
    ASSERT_EQ(mediumBitrate, mediumAfterCustom.recording.bitrateMbps);
}

// When a user enables recording with an empty schedule, special notification should appear.
TEST_F(CameraSettingsDialogStateReducerTest, recordingAlertIfScheduleIsEmpty)
{
    const auto camera = createCamera();
    camera->setScheduleTasks(makeEmptySchedule());

    State initial = Reducer::loadCameras({}, {camera});
    ASSERT_FALSE(initial.recording.enabled());
    ASSERT_FALSE(initial.recordingHint.has_value());

    const auto makeRecordingState =
        [camera]
        {
            return Reducer::setRecordingEnabled(Reducer::loadCameras({}, {camera}), true);
        };

    const auto [r1, recordingEnabled] = makeRecordingState();
    ASSERT_TRUE(recordingEnabled.recording.enabled());
    ASSERT_TRUE(recordingEnabled.recordingHint.has_value());
    ASSERT_EQ(*recordingEnabled.recordingHint, State::RecordingHint::emptySchedule);

    // Warning should be hidden if we disable recording.
    const auto [r2, recordingDisabled] = Reducer::setRecordingEnabled(makeRecordingState().second, false);
    ASSERT_FALSE(recordingDisabled.recordingHint.has_value());

    // Warning should be hidden if we setup some schedule.
    State scheduleSet = Reducer::setSchedule(makeRecordingState().second, makeDefaultSchedule());
    ASSERT_FALSE(recordingDisabled.recordingHint.has_value());
}

// "Motion + Lo-Res" recording types must not be available if secondary stream is
// disabled on the expert settings page.
TEST_F(CameraSettingsDialogStateReducerTest, disableMotionLQBrushIfDualStreamingIsDisabled)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    const auto camera = createCamera();
    camera->setHasDualStreaming(true);

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    ASSERT_TRUE(initial.isMotionDetectionEnabled());
    ASSERT_TRUE(initial.isDualStreamingEnabled());

    State setMotionAndLowBrush = Reducer::setScheduleBrushRecordingType(
        std::move(initial),
        Qn::RecordingType::metadataAndLowQuality);
    ASSERT_EQ(setMotionAndLowBrush.recording.brush.recordingType,
        Qn::RecordingType::metadataAndLowQuality);

    State disabledDualStreaming = Reducer::setDualStreamingDisabled(
        std::move(setMotionAndLowBrush), true);
    ASSERT_TRUE(disabledDualStreaming.isMotionDetectionEnabled());
    ASSERT_FALSE(disabledDualStreaming.isDualStreamingEnabled());
    ASSERT_TRUE(disabledDualStreaming.recording.brush.recordingType
        != Qn::RecordingType::metadataAndLowQuality);
}

// "Disable secondary stream" option is independent from "Device optimization enabled" setting.
TEST_F(CameraSettingsDialogStateReducerTest, deviceControlDisabledDoesntAffectDualStreaming)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    const auto camera = createCamera();
    camera->setHasDualStreaming(true);

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    ASSERT_TRUE(initial.isDualStreamingEnabled());

    State disabledDualStreaming = Reducer::setDualStreamingDisabled(
        std::move(initial), true);
    ASSERT_FALSE(disabledDualStreaming.isDualStreamingEnabled());

    State disableDeviceOptimization = Reducer::setSettingsOptimizationEnabled(
        std::move(disabledDualStreaming), false);

    ASSERT_FALSE(disableDeviceOptimization.isDualStreamingEnabled());
}

/**
 * Switch "Disable secondary stream" option on;
 *      Check that MD is in the mid state on primary stream.
 * Apply changes;
 *      Check that MD is in the mid state on primary stream.
 * Switch "Disable secondary stream" option off;
 *      Check that MD is turned ON on secondary stream.
 * Apply changes;
 *      Check that MD is turned ON on secondary stream.
 */
TEST_F(CameraSettingsDialogStateReducerTest, reEnableDualStreaming)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    const auto camera = createCamera();
    camera->setHasDualStreaming(true);

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    ASSERT_TRUE(initial.isDualStreamingEnabled());
    ASSERT_TRUE(initial.motion.enabled.hasValue());
    ASSERT_TRUE(initial.motion.enabled.get());
    ASSERT_EQ(initial.motion.stream.getBase(), State::StreamIndex::secondary);

    // Turn dual streaming off.
    State disabledDualStreaming = Reducer::setDualStreamingDisabled(
        std::move(initial), true);

    ASSERT_FALSE(disabledDualStreaming.isDualStreamingEnabled());
    ASSERT_TRUE(disabledDualStreaming.isMotionImplicitlyDisabled());
    ASSERT_TRUE(disabledDualStreaming.motion.enabled.hasValue());
    ASSERT_TRUE(disabledDualStreaming.motion.enabled.get());
    ASSERT_EQ(disabledDualStreaming.motion.stream.getBase(), State::StreamIndex::secondary);
    ASSERT_EQ(disabledDualStreaming.effectiveMotionStream(), State::StreamIndex::primary);

    // Press Apply button.
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(
        disabledDualStreaming, {camera});
    State reloadedDiabledDualStreaming = Reducer::loadCameras({}, {camera});

    ASSERT_FALSE(reloadedDiabledDualStreaming.isDualStreamingEnabled());
    ASSERT_TRUE(reloadedDiabledDualStreaming.isMotionImplicitlyDisabled());
    ASSERT_TRUE(reloadedDiabledDualStreaming.motion.enabled.hasValue());
    ASSERT_TRUE(reloadedDiabledDualStreaming.motion.enabled.get());
    ASSERT_EQ(reloadedDiabledDualStreaming.motion.stream.getBase(), State::StreamIndex::primary);
    ASSERT_EQ(reloadedDiabledDualStreaming.effectiveMotionStream(), State::StreamIndex::primary);

    // Turn dual streaming on.
    State enabledDualStreaming = Reducer::setDualStreamingDisabled(
        std::move(reloadedDiabledDualStreaming), false);

    ASSERT_TRUE(enabledDualStreaming.isDualStreamingEnabled());
    ASSERT_TRUE(enabledDualStreaming.isMotionImplicitlyDisabled());
    ASSERT_TRUE(enabledDualStreaming.motion.enabled.hasValue());
    ASSERT_TRUE(enabledDualStreaming.motion.enabled.get());
    ASSERT_EQ(enabledDualStreaming.motion.stream.getBase(), State::StreamIndex::primary);
    ASSERT_EQ(enabledDualStreaming.effectiveMotionStream(), State::StreamIndex::primary);

    // Press Apply button.
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(enabledDualStreaming, {camera});
    State reloadedEnabledDualStreaming = Reducer::loadCameras(std::move(enabledDualStreaming), {camera});

    ASSERT_TRUE(reloadedEnabledDualStreaming.isDualStreamingEnabled());
    ASSERT_FALSE(reloadedEnabledDualStreaming.isMotionImplicitlyDisabled());
    ASSERT_TRUE(reloadedEnabledDualStreaming.motion.enabled.hasValue());
    ASSERT_TRUE(reloadedEnabledDualStreaming.motion.enabled.get());
    ASSERT_EQ(reloadedEnabledDualStreaming.motion.stream.getBase(), State::StreamIndex::secondary);
    ASSERT_EQ(reloadedEnabledDualStreaming.effectiveMotionStream(), State::StreamIndex::secondary);
}

TEST_F(CameraSettingsDialogStateReducerTest, tooHighResolutionCheck)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    const auto camera = createCamera();
    camera->setProperty(ResourcePropertyKey::kSupportedMotion, QStringLiteral("softwaregrid"));
    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    ASSERT_TRUE(initial.motion.supportsSoftwareDetection);
    ASSERT_EQ(initial.motion.streamAlert, std::nullopt);

    camera->setStreamResolution(nx::vms::api::StreamIndex::primary, kTooBigResolution);
    camera->setStreamResolution(nx::vms::api::StreamIndex::secondary, kTooBigResolution);
    State tooHighResolution = Reducer::handleMediaStreamsChanged(std::move(initial), {camera});
    ASSERT_TRUE(tooHighResolution.motion.streamAlert.has_value());
    ASSERT_EQ(*tooHighResolution.motion.streamAlert, State::MotionStreamAlert::implicitlyDisabled);
}

TEST_F(CameraSettingsDialogStateReducerTest, tooHighResolutionHardwareDetectionCheck)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    const auto camera = createCamera();
    camera->setProperty(ResourcePropertyKey::kSupportedMotion, QStringLiteral("hardwaregrid"));
    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    ASSERT_FALSE(initial.motion.supportsSoftwareDetection);
    ASSERT_EQ(initial.motion.streamAlert, std::nullopt);

    camera->setStreamResolution(nx::vms::api::StreamIndex::primary, kTooBigResolution);
    camera->setStreamResolution(nx::vms::api::StreamIndex::secondary, kTooBigResolution);
    State tooHighResolution = Reducer::handleMediaStreamsChanged(std::move(initial), {camera});
    ASSERT_EQ(initial.motion.streamAlert, std::nullopt);
}

TEST_F(CameraSettingsDialogStateReducerTest, scheduleAlerts)
{
    // Global option to allow expert settings editing must be enabled first.
    State enableDeviceOptimization = Reducer::setSettingsOptimizationEnabled({}, true);

    const auto camera = createCamera();
    camera->setHasDualStreaming(true);
    camera->setScheduleEnabled(true);

    constexpr int kSomeFps = 10;

    State initial = Reducer::loadCameras(std::move(enableDeviceOptimization), {camera});
    EXPECT_TRUE(initial.motion.supportsSoftwareDetection);
    EXPECT_TRUE(initial.isMotionDetectionEnabled());
    EXPECT_FALSE(initial.isObjectDetectionSupported());

    State withSchedule = Reducer::setSchedule(std::move(initial), makeSchedule(
        Qn::RecordingType::metadataAndLowQuality,
        Qn::RecordingMetadataTypes(Qn::RecordingMetadataType::motion),
        Qn::StreamQuality::normal,
        kSomeFps));

    EXPECT_EQ(withSchedule.scheduleAlerts, State::ScheduleAlerts());

    State disabledDualStreaming = Reducer::setDualStreamingDisabled(std::move(withSchedule), true);
    EXPECT_EQ(disabledDualStreaming.scheduleAlerts,
        State::ScheduleAlerts{State::ScheduleAlert::noMotionLowRes});

    State withNoDualSchedule = Reducer::setSchedule(std::move(disabledDualStreaming),
        makeSchedule(
            Qn::RecordingType::metadataOnly,
            Qn::RecordingMetadataTypes(Qn::RecordingMetadataType::motion),
            Qn::StreamQuality::normal,
            kSomeFps));

    EXPECT_EQ(withNoDualSchedule.scheduleAlerts,
        State::ScheduleAlerts{State::ScheduleAlert::noMotion});

    State noMotion = Reducer::setMotionDetectionEnabled(std::move(withNoDualSchedule), false);
    EXPECT_EQ(noMotion.scheduleAlerts, State::ScheduleAlerts{State::ScheduleAlert::noMotion});

    State withObjectDetectionSchedule = Reducer::setSchedule(std::move(noMotion),
        makeSchedule(
            Qn::RecordingType::metadataOnly,
            Qn::RecordingMetadataTypes(Qn::RecordingMetadataType::objects),
            Qn::StreamQuality::normal,
            kSomeFps));
    EXPECT_EQ(withObjectDetectionSchedule.scheduleAlerts,
        State::ScheduleAlerts{State::ScheduleAlert::noObjects});

    State withMotionAndObjectDetectionSchedule = Reducer::setSchedule(
        std::move(withObjectDetectionSchedule),
        makeSchedule(
            Qn::RecordingType::metadataOnly,
            Qn::RecordingMetadataTypes(
                Qn::RecordingMetadataType::motion | Qn::RecordingMetadataType::objects),
            Qn::StreamQuality::normal,
            kSomeFps));

    EXPECT_EQ(withMotionAndObjectDetectionSchedule.scheduleAlerts,
        State::ScheduleAlerts{State::ScheduleAlert::noBoth});

    State withRecorgingAlways = Reducer::setSchedule(
        std::move(withMotionAndObjectDetectionSchedule),
        makeSchedule(Qn::RecordingType::always,
            Qn::RecordingMetadataTypes(),
            Qn::StreamQuality::normal,
            kSomeFps));

    EXPECT_EQ(withRecorgingAlways.scheduleAlerts, State::ScheduleAlerts());
}

TEST_F(CameraSettingsDialogStateReducerTest, noScheduleAlertsWithDifferentDevices /*VMS-15867*/)
{
    constexpr int kSomeFps = 10;

    const auto camera = createCamera();
    camera->setHasDualStreaming(true);
    camera->setScheduleTasks(makeSchedule(
        Qn::RecordingType::metadataAndLowQuality,
        Qn::RecordingMetadataTypes(Qn::RecordingMetadataType::motion),
        Qn::StreamQuality::normal,
        kSomeFps));

    CameraResourceStubPtr dtsBasedCamera = createNvrCamera();

    State noAlert = Reducer::loadCameras({}, {camera, dtsBasedCamera});
    ASSERT_TRUE(!noAlert.scheduleAlerts);
}

// Validate VMS-17723 scenario: each camera schedule is OK, but reducer suggests to disable motion.
TEST_F(CameraSettingsDialogStateReducerTest, DISABLED_skipMotionAlertIfScheduleIsNotToBeModified)
{
    // Given camera without motion support and with valid schedule.
    const auto cameraWithNoMotion = createCamera();

    cameraWithNoMotion->setDisableDualStreaming(true);
    cameraWithNoMotion->setScheduleTasks(makeSchedule(Qn::RecordingType::always,
        Qn::RecordingMetadataTypes()));
    EXPECT_TRUE(!Reducer::loadCameras({}, {cameraWithNoMotion}).scheduleAlerts);

    // Given camera with motion support and with valid motion-only schedule.
    const auto cameraWithMotion = createCamera();

    cameraWithMotion->setScheduleTasks(makeSchedule(Qn::RecordingType::metadataOnly,
        Qn::RecordingMetadataTypes(Qn::RecordingMetadataType::motion)));
    EXPECT_TRUE(!Reducer::loadCameras({}, {cameraWithMotion}).scheduleAlerts);

    // Selecting both cameras must not emit any schedule alerts.
    const auto initial = Reducer::loadCameras({}, {cameraWithNoMotion, cameraWithMotion});
    ASSERT_TRUE(!initial.scheduleAlerts);
}

// "virtualCameraIgnoreTimeZone" property test
TEST_F(CameraSettingsDialogStateReducerTest, virtualCameraIgnoreTimeZone)
{
    // Test default state value.
    State state1 = Reducer::setSettingsOptimizationEnabled({}, true);
    ASSERT_FALSE(state1.virtualCameraIgnoreTimeZone);
    state1 = Reducer::setVirtualCameraIgnoreTimeZone(std::move(state1), true);
    ASSERT_TRUE(state1.virtualCameraIgnoreTimeZone);

    // Test default camera state.
    const auto camera = createCamera();

    ASSERT_FALSE(camera->virtualCameraIgnoreTimeZone());
    State state2 = Reducer::loadCameras({}, {camera});
    ASSERT_FALSE(state2.virtualCameraIgnoreTimeZone);

    // Test value ignoring for non virtual cameras.
    ASSERT_EQ(state2.devicesDescription.isVirtualCamera, CombinedValue::None);
    state2 = Reducer::setVirtualCameraIgnoreTimeZone(std::move(state2), true);
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state2, {camera});
    ASSERT_FALSE(camera->virtualCameraIgnoreTimeZone());

    // Test value setting for virtual cameras.
    camera->addFlags(Qn::virtual_camera);
    State state3 = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(state3.devicesDescription.isVirtualCamera, CombinedValue::All);
    state3 = Reducer::setVirtualCameraIgnoreTimeZone(std::move(state3), true);
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state3, {camera});
    ASSERT_TRUE(camera->virtualCameraIgnoreTimeZone());
}

TEST_F(CameraSettingsDialogStateReducerTest, analyzedStreamIndexAndVisibility)
{
    State initialState = Reducer::loadCameras({}, {createCamera()});

    // By default for some yet unknown device agent stream selection is considered enabled.
    const auto testEngineId = QnUuid::createUuid();
    ASSERT_TRUE(initialState.analyticsStreamSelectionEnabled(testEngineId));

    // Set analytics stream index remotely to secondary.
    auto state1 = Reducer::setAnalyticsStreamIndex(std::move(initialState), testEngineId,
        State::StreamIndex::secondary, ModificationSource::remote);

    ASSERT_TRUE(state1.analyticsStreamSelectionEnabled(testEngineId));
    ASSERT_FALSE(state1.analytics.streamByEngineId.value(testEngineId).hasUser());
    ASSERT_EQ(state1.analytics.streamByEngineId.value(testEngineId)(), State::StreamIndex::secondary);

    // Change analytics stream index locally to primary.
    auto state2 = Reducer::setAnalyticsStreamIndex(std::move(state1), testEngineId,
        State::StreamIndex::primary, ModificationSource::local);

    ASSERT_TRUE(state2.analyticsStreamSelectionEnabled(testEngineId));
    ASSERT_TRUE(state2.analytics.streamByEngineId.value(testEngineId).hasUser());
    ASSERT_EQ(state2.analytics.streamByEngineId.value(testEngineId)(), State::StreamIndex::primary);

    // Change analytics stream index remotely to undefined, meaning stream selection is disabled.
    auto state3 = Reducer::setAnalyticsStreamIndex(std::move(state2), testEngineId,
        State::StreamIndex::undefined, ModificationSource::remote);

    ASSERT_FALSE(state3.analyticsStreamSelectionEnabled(testEngineId));
    ASSERT_FALSE(state3.analytics.streamByEngineId.value(testEngineId).hasUser());
    ASSERT_EQ(state3.analytics.streamByEngineId.value(testEngineId)(), State::StreamIndex::undefined);
}

/**
 * Make sure when the quality limits is changed (e.g. 'Low' become unavailable), brush should be
 * changed accordingly.
 */
TEST_F(CameraSettingsDialogStateReducerTest, qualityLimitsChangeKeepsBrush)
{
    State initialState = Reducer::loadCameras({}, {createCamera()});

    EXPECT_EQ(initialState.recording.minRelevantQuality, nx::vms::api::StreamQuality::lowest);
    EXPECT_EQ(initialState.recording.brush.isAutoBitrate(), true);

    // Actual default recording table value is motion-only, high quality.
    EXPECT_EQ(initialState.recording.brush.streamQuality, nx::vms::api::StreamQuality::high);

    // With minimal fps we can have the only one bitrate, so it is called the best.
    State minFpsState = Reducer::setScheduleBrushFps(std::move(initialState), 1);
    EXPECT_EQ(minFpsState.recording.minRelevantQuality, nx::vms::api::StreamQuality::highest);
    EXPECT_EQ(minFpsState.recording.brush.streamQuality, nx::vms::api::StreamQuality::highest);
}

//-------------------------------------------------------------------------------------------------
// Web Page

// Camera web page address is basically a camera API url without query and path. Also there are
// some options that affect it:
// - System Settings -> useHttpsOnlyForCameras option forces using https protocol
// - http_port property defines custom port (high priority)
// - http_port query parameter defines custom port (low priority)
// - default ports (80 for http and 443 for https) are omitted
TEST_F(CameraSettingsDialogStateReducerTest, webPageAddress)
{
    auto camera = createCamera();
    camera->setUrl("https://example.com:777/path?query=true");
    State state = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(state.singleCameraProperties.settingsUrl, QString("https://example.com:777/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageAddressDefaultPortHttps)
{
    auto camera = createCamera();
    camera->setUrl("https://example.com:443/path?query=true");
    State state = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(state.singleCameraProperties.settingsUrl, QString("https://example.com/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageAddressDefaultPortHttp)
{
    auto camera = createCamera();
    camera->setUrl("http://example.com:80/path?query=true");
    State state = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(state.singleCameraProperties.settingsUrl, QString("http://example.com/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageForceHttpsSubstitution)
{
    auto camera = createCamera();
    camera->systemContext()->globalSettings()->setUseHttpsOnlyForCameras(true);
    camera->setUrl("http://example.com:443/path?query=true");
    State state = Reducer::loadCameras({}, {camera});
    ASSERT_EQ(state.singleCameraProperties.settingsUrl, QString("https://example.com/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageAddressQueryCustomPort)
{
    auto camera = createCamera();
    camera->setUrl("https://example.com/path?query=true&http_port=277");
    State state = Reducer::loadCameras({}, {camera});
    EXPECT_TRUE(state.expert.customWebPagePort.equals(0));
    ASSERT_EQ(state.singleCameraProperties.settingsUrl, QString("https://example.com:277/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageAddressPropertyCustomPort)
{
    auto camera = createCamera();
    camera->setUrl("https://example.com/path?query=true&http_port=277");
    camera->setCustomWebPagePort(555);
    State state = Reducer::loadCameras({}, {camera});
    EXPECT_TRUE(state.expert.customWebPagePort.equals(555));
    EXPECT_FALSE(state.isDefaultExpertSettings);
    ASSERT_EQ(state.singleCameraProperties.settingsUrl, QString("https://example.com:555/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageSetCustomPort)
{
    auto camera = createCamera();

    camera->setUrl("https://example.com:777");
    State state = Reducer::loadCameras({}, {camera});
    EXPECT_TRUE(state.isDefaultExpertSettings);
    State changed = Reducer::setCustomWebPagePort(std::move(state), 555);
    EXPECT_TRUE(changed.expert.customWebPagePort.equals(555));
    EXPECT_FALSE(changed.isDefaultExpertSettings);
    ASSERT_EQ(changed.singleCameraProperties.settingsUrl, QString("https://example.com:555/"));
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageApplyCustomPort)
{
    auto camera = createCamera();
    State state = Reducer::loadCameras({}, {camera});
    State changed = Reducer::setCustomWebPagePort(std::move(state), 555);
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(changed, {camera});
    ASSERT_EQ(camera->customWebPagePort(), 555);
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageAddressCleanCustomPort)
{
    auto camera = createCamera();
    camera->setUrl("https://example.com:777/");
    camera->setCustomWebPagePort(555);
    State state = Reducer::loadCameras({}, {camera});
    State changed = Reducer::setCustomWebPagePort(std::move(state), 0);
    EXPECT_TRUE(changed.expert.customWebPagePort.equals(0));
    ASSERT_EQ(changed.singleCameraProperties.settingsUrl, QString("https://example.com:777/"));
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(changed, {camera});
    ASSERT_EQ(camera->customWebPagePort(), 0);
}

TEST_F(CameraSettingsDialogStateReducerTest, webPageMultipleCamerasCustomPort)
{
    auto camera1 = createCamera();
    camera1->setCustomWebPagePort(555);
    auto camera2 = createCamera();
    QnVirtualCameraResourceList cameras{camera1, camera2};
    State state = Reducer::loadCameras({}, cameras);
    EXPECT_FALSE(state.expert.customWebPagePort.hasValue());
    State changed = Reducer::setCustomWebPagePort(std::move(state), 80);
    EXPECT_TRUE(changed.expert.customWebPagePort.equals(80));
}

//-------------------------------------------------------------------------------------------------
// Rotation

TEST_F(CameraSettingsDialogStateReducerTest, cameraRotationSaveLoad)
{
    using StandardRotation = nx::vms::client::core::StandardRotation;

    auto camera = createCamera();
    QnVirtualCameraResourceList cameras{camera};
    State first = Reducer::loadCameras({}, cameras);
    ASSERT_TRUE(first.imageControl.rotationAvailable);
    ASSERT_TRUE(first.imageControl.rotation.hasValue());
    ASSERT_EQ(first.imageControl.rotation.get(), StandardRotation::rotate0);

    State second = Reducer::setCustomRotation(std::move(first), StandardRotation::rotate90);
    CameraSettingsDialogStateConversionFunctions::applyStateToCameras(second, cameras);

    State third = Reducer::loadCameras({}, cameras);
    EXPECT_TRUE(third.imageControl.rotationAvailable);
    EXPECT_TRUE(third.imageControl.rotation.hasValue());
    EXPECT_EQ(third.imageControl.rotation.get(), StandardRotation::rotate90);
}

TEST_F(CameraSettingsDialogStateReducerTest, resetDeviceAgentData)
{
    const auto engineId = QnUuid::createUuid();

    const State::AnalyticsSettings::EngineSettings initialSettings = {
        .model = {{"name", "base model"}},
        .values = {
            .base = QJsonObject{{"name", "base value"}},
            .user = QJsonObject{{"name", "user value"}}
        }
    };

    auto initial =
        [&]{ return State{ .analytics = { .settingsByEngineId = {{engineId, initialSettings}}}}; };

    const QJsonObject newBaseValues = {{"name", "new base value"}};
    const QJsonObject newUserValues = {{"name", "new user value"}};
    const QJsonObject newModel = {{"name", "new model"}};

    auto [result1, baseReset]  = Reducer::resetDeviceAgentData(
        initial(),
        engineId,
        {.model = newModel, .values = newBaseValues},
        /*replaceUser*/ false);

    const auto& baseResetSettings = baseReset.analytics.settingsByEngineId[engineId];
    EXPECT_EQ(baseResetSettings.values.base, newBaseValues);
    EXPECT_FALSE(baseResetSettings.values.user.has_value());
    EXPECT_EQ(baseResetSettings.model, newModel);

    auto [result2, userReplace] = Reducer::resetDeviceAgentData(
        initial(),
        engineId,
        {.model = newModel, .values = newUserValues},
        /*replaceUser*/ true);

    const auto& userReplaceSettings = userReplace.analytics.settingsByEngineId[engineId];
    EXPECT_EQ(userReplaceSettings.values.base, initialSettings.values.base);
    EXPECT_EQ(userReplaceSettings.values.user, newUserValues);
    EXPECT_EQ(userReplaceSettings.model, newModel);
}

//-------------------------------------------------------------------------------------------------
// Analytics

TEST_F(CameraSettingsDialogStateReducerTest, enginelicenseChangesHandledProperly)
{
    auto state = Reducer::loadCameras({}, {createCamera()});
    bool isChanged{};

    QList<core::AnalyticsEngineInfo> engines;
    engines.push_back(makeAnalyticsEngine(/*isLicenseRequired*/ true));
    std::tie(isChanged, state) = Reducer::setAnalyticsEngines(std::move(state), engines);

    state = Reducer::handleOverusedEngines(std::move(state), {engines.front().id});
    EXPECT_TRUE(state.analytics.userEnabledEngines.get().empty());
}

} // namespace test
} // namespace nx::vms::client::desktop
