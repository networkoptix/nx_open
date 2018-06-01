#include "camera_settings_dialog_state_conversion_functions.h"
#include "../redux/camera_settings_dialog_state.h"

#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>

#include <nx/client/core/motion/motion_grid.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

using State = CameraSettingsDialogState;
using Cameras = QnVirtualCameraResourceList;

QString boolToPropertyStr(bool value)
{
    return value ? lit("1") : lit("0");
}

void setMinRecordingDays(
    const State::RecordingDays& value,
    const Cameras& cameras)
{
    if (!value.same)
        return;
    int actualValue = value.absoluteValue;
    NX_ASSERT(actualValue > 0);
    if (actualValue == 0)
        actualValue = nx::vms::api::kDefaultMinArchiveDays;
    if (value.automatic)
        actualValue = -actualValue;

    for (const auto& camera: cameras)
        camera->setMinDays(actualValue);
}

void setMaxRecordingDays(
    const State::RecordingDays& value,
    const Cameras& cameras)
{
    if (!value.same)
        return;
    int actualValue = value.absoluteValue;
    NX_ASSERT(actualValue > 0);
    if (actualValue == 0)
        actualValue = nx::vms::api::kDefaultMaxArchiveDays;
    if (value.automatic)
        actualValue = -actualValue;

    for (const auto& camera: cameras)
        camera->setMaxDays(actualValue);
}

void setCustomRotation(
    const Rotation& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_EXPECT(camera->hasVideo());
        if (!camera->hasVideo())
            continue;

        const QString rotationString = value.isValid() ? value.toString() : QString();
        camera->setProperty(QnMediaResource::rotationKey(), rotationString);
    }
}

void setCustomAspectRatio(
    const QnAspectRatio& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_EXPECT(camera->hasVideo() && !camera->hasFlags(Qn::wearable_camera));
        if (camera->hasVideo() && !camera->hasFlags(Qn::wearable_camera))
        {
            if (value.isValid())
                camera->setCustomAspectRatio(value);
            else
                camera->clearCustomAspectRatio();
        }
    }
}

void setSchedule(const ScheduleTasks& schedule, const Cameras& cameras)
{
    QnScheduleTaskList scheduleTasks;
    for (const auto& data: schedule)
        scheduleTasks << data;

    for (const auto& camera: cameras)
    {
        if (camera->isDtsBased())
            continue;

        camera->setScheduleTasks(scheduleTasks);
    }
}

void setRecordingBeforeThreshold(
    int value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
         camera->setRecordBeforeMotionSec(value);
}

void setRecordingAfterThreshold(
    int value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
         camera->setRecordAfterMotionSec(value);
}


void setRecordingEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setLicenseUsed(value);
}

void setDualStreamingDisabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->hasDualStreamingInternal())
            camera->setDisableDualStreaming(value);
    }
}

void setCameraControlDisabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setCameraControlDisabled(value);
}

void setUseBitratePerGOP(bool value, const Cameras& cameras)
{
    const auto valueStr = boolToPropertyStr(value);
    for (const auto& camera: cameras)
    {
        if (camera->bitratePerGopType() != Qn::BPG_Predefined)
            camera->setProperty(Qn::FORCE_BITRATE_PER_GOP, valueStr);
    }
}

void setPrimaryRecordingDisabled(bool value, const Cameras& cameras)
{
    const auto valueStr = boolToPropertyStr(value);

    for (const auto& camera: cameras)
        camera->setProperty(QnMediaResource::dontRecordPrimaryStreamKey(), valueStr);
}

void setSecondaryRecordingDisabled(bool value, const Cameras& cameras)
{
    const auto valueStr = boolToPropertyStr(value);
    for (const auto& camera: cameras)
    {
        if (camera->hasDualStreamingInternal())
            camera->setProperty(QnMediaResource::dontRecordSecondaryStreamKey(), valueStr);
    }
}

void setNativePtzPresetsDisabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->canDisableNativePtzPresets())
        {
            camera->setProperty(Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME,
                value ? lit("true") : QString());
        }
    }
}

void setRtpTransportType(vms::api::RtpTransportType value, const Cameras& cameras)
{
    const auto valueStr = value == vms::api::RtpTransportType::automatic
        ? QString()
        : QnLexical::serialized(value);

    for (const auto& camera: cameras)
        camera->setProperty(QnMediaResource::rtpTransportKey(), valueStr);
}

void setMotionStreamType(vms::api::MotionStreamType value, const Cameras& cameras)
{
    const auto isValueSupported =
        [value](const QnVirtualCameraResourcePtr& camera) -> bool
        {
            switch (value)
            {
                case vms::api::MotionStreamType::secondary:
                    return camera->hasDualStreamingInternal();
                case vms::api::MotionStreamType::edge:
                    return camera->hasCameraCapabilities(Qn::RemoteArchiveCapability);
                default:
                    return true;
            }
        };

    const auto valueStr = value == vms::api::MotionStreamType::automatic
        ? QString()
        : QnLexical::serialized(value);

    for (const auto& camera: cameras)
    {
        if (isValueSupported(camera))
            camera->setProperty(QnMediaResource::motionStreamKey(), valueStr);
    }
}

void setWearableMotionEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (!camera->hasFlags(Qn::wearable_camera))
            continue;

        NX_ASSERT(camera->getDefaultMotionType() == Qn::MotionType::MT_SoftwareGrid);
        camera->setMotionType(value
            ? Qn::MotionType::MT_SoftwareGrid
            : Qn::MotionType::MT_NoMotion);
    }
}

void setWearableMotionSensitivity(int value, const Cameras& cameras)
{
    QnMotionRegion region;
    region.addRect(value, QRect(0, 0, core::MotionGrid::kWidth, core::MotionGrid::kHeight));

    for (const auto& camera: cameras)
    {
        if (!camera->hasFlags(Qn::wearable_camera))
            continue;

        NX_ASSERT(camera->getVideoLayout()->channelCount() == 1);
        camera->setMotionRegion(region, 0);
    }
}

} // namespace

void CameraSettingsDialogStateConversionFunctions::applyStateToCameras(
    const State& state, const Cameras& cameras)
{
    if (state.isSingleCamera())
    {
        auto camera = cameras.first();
        camera->setName(state.singleCameraProperties.name());

        camera->setDewarpingParams(state.fisheyeSettings());

        const int logicalId = state.singleCameraSettings.logicalId();
        camera->setLogicalId((logicalId > 0) ? QString::number(logicalId) : QString());

        if (state.devicesDescription.hasMotion == State::CombinedValue::All)
        {
            camera->setMotionType(state.singleCameraSettings.enableMotionDetection()
                ? camera->getDefaultMotionType()
                : Qn::MotionType::MT_NoMotion);

            camera->setMotionRegionList(state.singleCameraSettings.motionRegionList());
        }

        if (camera->isIOModule() && state.devicesDescription.isIoModule == State::CombinedValue::All)
        {
            camera->setProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME, QnLexical::serialized(
                state.singleIoModuleSettings.visualStyle()));

            const auto ioPortDataList = state.singleIoModuleSettings.ioPortsData();
            if (!ioPortDataList.empty()) //< Can happen if it's just discovered unauthorized module.
                camera->setIOPorts(ioPortDataList);
        }
    }

    if (state.devicesDescription.isWearable == State::CombinedValue::All)
    {
        if (state.wearableMotion.enabled.hasValue())
        {
            setWearableMotionEnabled(state.wearableMotion.enabled(), cameras);

            if (state.wearableMotion.enabled() && state.wearableMotion.sensitivity.hasValue())
                setWearableMotionSensitivity(state.wearableMotion.sensitivity(), cameras);
        }
    }

    setMinRecordingDays(state.recording.minDays, cameras);
    setMaxRecordingDays(state.recording.maxDays, cameras);

    if (state.imageControl.aspectRatio.hasValue())
        setCustomAspectRatio(state.imageControl.aspectRatio(), cameras);

    if (state.imageControl.rotation.hasValue())
        setCustomRotation(state.imageControl.rotation(), cameras);

    if (state.recording.enabled.hasValue())
        setRecordingEnabled(state.recording.enabled(), cameras);

    if (state.recording.schedule.hasValue())
        setSchedule(state.recording.schedule(), cameras);

    if (state.recording.thresholds.beforeSec.hasValue())
        setRecordingBeforeThreshold(state.recording.thresholds.beforeSec(), cameras);

    if (state.recording.thresholds.afterSec.hasValue())
        setRecordingAfterThreshold(state.recording.thresholds.afterSec(), cameras);

    if (state.settingsOptimizationEnabled)
    {
        if (state.expert.dualStreamingDisabled.hasValue())
            setDualStreamingDisabled(state.expert.dualStreamingDisabled(), cameras);

        if (state.expert.useBitratePerGOP.hasValue()
            && state.devicesDescription.hasPredefinedBitratePerGOP == State::CombinedValue::None
            && !state.expert.cameraControlDisabled.valueOr(false))
        {
            setUseBitratePerGOP(state.expert.useBitratePerGOP(), cameras);
        }

        if (state.devicesDescription.isArecontCamera == State::CombinedValue::None
            && state.expert.cameraControlDisabled.hasValue())
        {
            setCameraControlDisabled(state.expert.cameraControlDisabled(), cameras);
        }
    }

    if (state.expert.primaryRecordingDisabled.hasValue())
        setPrimaryRecordingDisabled(state.expert.primaryRecordingDisabled(), cameras);

    if (state.expert.secondaryRecordingDisabled.hasValue())
        setSecondaryRecordingDisabled(state.expert.secondaryRecordingDisabled(), cameras);

    if (state.devicesDescription.supportsMotionStreamOverride == State::CombinedValue::All
        && state.expert.motionStreamType.hasValue())
    {
        setMotionStreamType(state.expert.motionStreamType(), cameras);
    }

    if (state.devicesDescription.canDisableNativePtzPresets != State::CombinedValue::None
        && state.expert.nativePtzPresetsDisabled.hasValue())
    {
        setNativePtzPresetsDisabled(state.expert.nativePtzPresetsDisabled(), cameras);
    }

    if (state.expert.rtpTransportType.hasValue())
        setRtpTransportType(state.expert.rtpTransportType(), cameras);
}

} // namespace desktop
} // namespace client
} // namespace nx
