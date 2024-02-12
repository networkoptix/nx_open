// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <nx/reflect/enum_string_conversion.h>
#include <nx/reflect/to_string.h>

#include "camera_settings_test_fixture.h"

namespace nx::vms::client::desktop {

std::ostream& operator<<(std::ostream& os, CameraSettingsDialogState::MetadataRadioButton value)
{
    switch (value)
    {
        case CameraSettingsDialogState::MetadataRadioButton::motion:
            os << "motion";
            break;
        case CameraSettingsDialogState::MetadataRadioButton::objects:
            os << "objects";
            break;
        case CameraSettingsDialogState::MetadataRadioButton::both:
            os << "both";
            break;
    }
    return os;
}

namespace test {

using namespace nx::vms::api;

class CameraSettingsDialogScheduleTest: public CameraSettingsTestFixture
{
protected:
    void whenRadioButtonIsChecked(State::MetadataRadioButton value)
    {
        dispatch(Reducer::setRecordingMetadataRadioButton, value);
    }

    void thenRadioButtonIsChecked(State::MetadataRadioButton value)
    {
        EXPECT_EQ(state().recording.metadataRadioButton, value);
    }

    void thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataTypes value)
    {
        EXPECT_EQ(state().recording.brush.metadataTypes, value);
    }

    void whenRecordingTypeIsSelected(RecordingType value)
    {
        dispatch(Reducer::setScheduleBrushRecordingType, value);
    }

    void thenScheduleBrushRecordingTypeIs(RecordingType value)
    {
        EXPECT_EQ(state().recording.brush.recordingType, value);
    }

    void whenUserDisablesAnalyticsEngine()
    {
        dispatch(Reducer::setUserEnabledAnalyticsEngines, QSet<nx::Uuid>());
    }

    QnScheduleTaskList getFilledSchedule(
        RecordingType recordingType,
        RecordingMetadataTypes metadataTypes,
        StreamQuality streamQuality = StreamQuality::normal,
        int fps = 0,
        int bitrateKbps = 0)
    {
        using namespace std::chrono;

        QnScheduleTaskList schedule;
        for (qint8 day = 1; day <= 7; ++day)
        {
            schedule.push_back({
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
        return schedule;
    }

    void whenScheduleFilledWith(
        RecordingType recordingType,
        RecordingMetadataTypes metadataTypes,
        StreamQuality streamQuality = StreamQuality::normal,
        int fps = 0,
        int bitrateKbps = 0)
    {
        auto schedule =
            getFilledSchedule(recordingType, metadataTypes, streamQuality, fps, bitrateKbps);
        dispatch(Reducer::setSchedule, schedule);
    }

    void thenScheduleAlertsAre(State::ScheduleAlerts value)
    {
        EXPECT_EQ(state().scheduleAlerts, value);
    }

    void thenScheduleWarningInformerIsShown(bool value = true)
    {
        const auto cameras = this->cameras();

        auto isScheduleValid =
            [](const QnVirtualCameraResourcePtr& camera)
            {
                return !camera->isScheduleEnabled()
                    || !camera->supportsSchedule()
                    || camera->canApplySchedule(camera->getScheduleTasks());
            };

        const bool informerIsShown = std::any_of(cameras.cbegin(), cameras.cend(),
            [isScheduleValid](const auto& camera)
            {
                return !isScheduleValid(camera);
            });
        EXPECT_EQ(value, informerIsShown);
    }

    void whenMotionStreamIsSelected(StreamIndex index)
    {
        dispatch(Reducer::setMotionStream, index);
    }

    void thenScheduleStreamQualityIs(StreamQuality value)
    {
        EXPECT_EQ(state().recording.brush.streamQuality, value);
    }
};

TEST_F(CameraSettingsDialogScheduleTest, recordingTypeChangeAffectsSchedule)
{
    givenCamera();
    // By default when dialog is opened, schedule is filled with motion, but "Record always" is
    // selected.
    thenScheduleBrushRecordingTypeIs(RecordingType::always);

    static const std::vector<RecordingType> kAllowedRecordingTypes{
        RecordingType::always,
        RecordingType::metadataOnly,
        RecordingType::never,
        RecordingType::metadataAndLowQuality,
    };

    for (auto recordingType: kAllowedRecordingTypes)
    {
        whenRecordingTypeIsSelected(recordingType);
        thenScheduleBrushRecordingTypeIs(recordingType);
    }
}

// When dialog is opened, record always is selected, motion radiobutton is checked
TEST_F(CameraSettingsDialogScheduleTest, recordAlwaysAndMotionButtonByDefault)
{
    givenCamera();
    thenScheduleBrushRecordingTypeIs(RecordingType::always);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::none);
}

// When dialog is opened and camera does not support motion, record always is selected, objects
// radiobutton is checked
TEST_F(CameraSettingsDialogScheduleTest, recordAlwaysAndObjectsButtonByDefaultIfNoMotion)
{
    givenCamera(CameraFeature::objects); //< Motion is not supported.
    thenScheduleBrushRecordingTypeIs(RecordingType::always);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::none);
}

// When dialog is opened and camera does not support neither motion nor objects, record always is
// selected, motion radiobutton is checked
TEST_F(CameraSettingsDialogScheduleTest, recordAlwaysAndMotionsButtonByDefaultIfNoMetadata)
{
    givenCamera(CameraFeature::none); //< Motion and objects are not supported.
    thenScheduleBrushRecordingTypeIs(RecordingType::always);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::none);
}

// Ensure that when user clicks on "Record always", selected "Objects" metadata types
// radiobutton kept intact.
TEST_F(CameraSettingsDialogScheduleTest, changingRecordingTypeDoesNotChangeRadioButton)
{
    givenCamera();

    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::motion);

    whenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::objects);

    whenRecordingTypeIsSelected(RecordingType::always);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::none);
}

// When camera does not support motion anymore, switch radiobutton from both to objects.
TEST_F(CameraSettingsDialogScheduleTest, disablingMotionChangesRadiobuttonFromBothToObjects)
{
    givenCamera();
    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    whenRadioButtonIsChecked(State::MetadataRadioButton::both);
    thenRadioButtonIsChecked(State::MetadataRadioButton::both);
    whenCameraFeaturesEnabled(CameraFeature::motion, false);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::objects);
}

// When camera does not support motion anymore, switch radiobutton from motion to objects.
TEST_F(CameraSettingsDialogScheduleTest, disablingMotionChangesRadiobuttonFromMotionToObjects)
{
    givenCamera();
    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    whenCameraFeaturesEnabled(CameraFeature::motion, false);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::objects);
}

// When camera does not support objects anymore, switch radiobutton from both to motion.
TEST_F(CameraSettingsDialogScheduleTest, disablingObjectsChangesRadiobuttonFromBothToMotion)
{
    givenCamera();
    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    whenRadioButtonIsChecked(State::MetadataRadioButton::both);
    thenRadioButtonIsChecked(State::MetadataRadioButton::both);
    whenCameraFeaturesEnabled(CameraFeature::objects, false);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::motion);
}

// When camera does not support objects anymore, switch radiobutton from objects to motion.
TEST_F(CameraSettingsDialogScheduleTest, disablingObjectsChangesRadiobuttonFromObjectsToMotion)
{
    givenCamera();
    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    whenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    whenCameraFeaturesEnabled(CameraFeature::objects, false);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::motion);
}

// When camera does not support motion and objects anymore, switch radiobutton to motion and
// recording type from metadata to record always.
TEST_F(CameraSettingsDialogScheduleTest, disablingBothChangesRadiobuttonFromObjectsToMotion)
{
    givenCamera();
    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    whenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    whenCameraFeaturesEnabled(CameraFeature::motion, false);
    whenCameraFeaturesEnabled(CameraFeature::objects, false);
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingTypeIs(RecordingType::always);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::none);
}

// When user disables last analytics engine with objects, switch radiobutton to motion.
TEST_F(CameraSettingsDialogScheduleTest, disablingEngineChangesRadiobuttonFromObjectsToMotion)
{
    givenCamera();
    whenRecordingTypeIsSelected(RecordingType::metadataOnly);
    whenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
    whenUserDisablesAnalyticsEngine();
    thenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    thenScheduleBrushRecordingMetadataTypesAre(RecordingMetadataType::motion);
}

// When user enables or disables recording, invalid modes banner should appear and hide.
TEST_F(CameraSettingsDialogScheduleTest, recordingSwitchAffectsScheduleBanner)
{
    givenCamera();
    whenMotionEnabled(false);
    whenScheduleFilledWith(RecordingType::metadataOnly, {RecordingMetadataType::motion});
    thenScheduleAlertsAre({});
    whenRecordingEnabled();
    thenScheduleAlertsAre({State::ScheduleAlert::noMotion});
}

// When second stream is disabled, motion radiobuttons should also be unavailable.
TEST_F(CameraSettingsDialogScheduleTest, motionStreamAffectsScheduleState)
{
    givenCamera();
    whenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    whenDualStreamingEnabled(false);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
}

// When camera becomes unauthorized, motion should be disabled.
TEST_F(CameraSettingsDialogScheduleTest, cameraStatusAffectsScheduleState)
{
    givenCamera();
    whenRadioButtonIsChecked(State::MetadataRadioButton::motion);
    whenCameraStatusChangedTo(ResourceStatus::unauthorized);
    thenRadioButtonIsChecked(State::MetadataRadioButton::objects);
}

// When secondary stream is disabled, schedule should be invalidated.
TEST_F(CameraSettingsDialogScheduleTest, handleSecondaryStreamDisablingChanges)
{
    givenCamera();
    whenScheduleFilledWith(RecordingType::metadataAndLowQuality, {RecordingMetadataType::motion});
    whenRecordingEnabled();
    thenScheduleAlertsAre({});

    whenSecondaryStreamRecordingEnabled(false);
    thenScheduleAlertsAre({State::ScheduleAlert::noMotionLowRes});

    whenChangesAreSaved();
    thenScheduleWarningInformerIsShown();
}

// When motion detection forced on primary stream, no alerts are shown.
TEST_F(CameraSettingsDialogScheduleTest, motionDetectionOnPrimaryStreamWorks)
{
    givenCamera();
    whenScheduleFilledWith(RecordingType::metadataAndLowQuality, {RecordingMetadataType::motion});
    whenRecordingEnabled();

    whenMotionStreamIsSelected(StreamIndex::primary);
    thenScheduleAlertsAre({});
}

TEST_F(CameraSettingsDialogScheduleTest, cameraRecordingBrushStreamingQualitySingleCamera)
{
    auto camera = givenCamera();
    camera->setScheduleTasks(getFilledSchedule(
        RecordingType::never, {RecordingMetadataType::motion}, StreamQuality::undefined));
    whenCamerasAreLoaded();
    thenScheduleStreamQualityIs(StreamQuality::high);
}

TEST_F(CameraSettingsDialogScheduleTest, cameraRecordingBrushStreamingQualityMultiCamera)
{
    auto camera1 = givenCamera();
    camera1->setScheduleTasks(getFilledSchedule(
        RecordingType::never, {RecordingMetadataType::motion}, StreamQuality::undefined));

    auto camera2 = givenCamera();
    camera2->setScheduleTasks(getFilledSchedule(
        RecordingType::never, {RecordingMetadataType::motion}, StreamQuality::high));
    whenCamerasAreLoaded();
    thenScheduleStreamQualityIs(StreamQuality::high);
}

} // namespace test
} // namespace nx::vms::client::desktop
