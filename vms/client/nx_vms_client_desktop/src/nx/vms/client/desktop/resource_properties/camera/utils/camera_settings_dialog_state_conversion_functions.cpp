#include "camera_settings_dialog_state_conversion_functions.h"
#include "../redux/camera_settings_dialog_state.h"

#include <core/ptz/ptz_preset.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>

#include <nx/client/core/motion/motion_grid.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::client::desktop {

namespace {

using State = CameraSettingsDialogState;
using Cameras = QnVirtualCameraResourceList;

//TODO #lbusygin: move conversion to resource layer
QString boolToPropertyStr(bool value)
{
    return value ? lit("1") : lit("0");
}

void setRecordingDays(
    const State::RecordingDays& minDays,
    const State::RecordingDays& maxDays,
    const Cameras& cameras)
{
    if (!minDays.automatic.hasValue() && !maxDays.automatic.hasValue())
        return;

    const auto absoluteDays =
        [](int compositeValue) { return qMax(qAbs(compositeValue), 1); };

    for (const auto& camera: cameras)
    {
        const int prevMinDays = camera->minDays();
        const int prevMaxDays = camera->maxDays();

        const bool autoMinDays = minDays.automatic.valueOr(prevMinDays < 0);
        const bool autoMaxDays = maxDays.automatic.valueOr(prevMaxDays < 0);
        int newMinDays = minDays.value.valueOr(absoluteDays(prevMinDays));
        int newMaxDays = maxDays.value.valueOr(absoluteDays(prevMaxDays));

        if (newMaxDays < newMinDays)
        {
            if (maxDays.value.hasValue())
                newMinDays = newMaxDays;
            else
                newMaxDays = newMinDays;
        }

        camera->setMinDays(autoMinDays ? -newMinDays : newMinDays);
        camera->setMaxDays(autoMaxDays ? -newMaxDays : newMaxDays);
    }
}

void setCustomRotation(
    const Rotation& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_ASSERT(camera->hasVideo());
        if (!camera->hasVideo())
            continue;

        const auto rotationString = (value != Rotation()) ? value.toString() : QString();
        camera->setProperty(QnMediaResource::rotationKey(), rotationString);
    }
}

void setCustomAspectRatio(
    const QnAspectRatio& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_ASSERT(camera->hasVideo() && !camera->hasFlags(Qn::wearable_camera));
        if (camera->hasVideo() && !camera->hasFlags(Qn::wearable_camera))
        {
            if (value.isValid())
                camera->setCustomAspectRatio(value);
            else
                camera->clearCustomAspectRatio();
        }
    }
}

void setSchedule(const QnScheduleTaskList& schedule, const Cameras& cameras)
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
        camera->setProperty(ResourcePropertyKey::kBitratePerGOP, valueStr);
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

void setPreferredPtzPresetType(nx::core::ptz::PresetType value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->canSwitchPtzPresetTypes())
            camera->setUserPreferredPtzPresetType(value);
    }
}

void setForcedPtzCapabilities(std::optional<bool> panTilt, std::optional<bool> zoom,
    const Cameras& cameras)
{
    if (!panTilt && !zoom)
        return;

    for (const auto& camera: cameras)
    {
        if (!camera->isUserAllowedToModifyPtzCapabilities())
            continue;

        auto capabilities = camera->ptzCapabilitiesAddedByUser();
        if (panTilt)
            capabilities.setFlag(Ptz::ContinuousPanTiltCapabilities, *panTilt);
        if (zoom)
            capabilities.setFlag(Ptz::ContinuousZoomCapability, *zoom);

        camera->setPtzCapabilitiesAddedByUser(capabilities);
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

void setTrustCameraTime(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setTrustCameraTime(value);
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

void setAudioEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setAudioEnabled(value);
}

void setCredentials(const QnVirtualCameraResourcePtr& camera, const QAuthenticator& authenticator)
{
    if ((camera->isMultiSensorCamera() || camera->isNvr()) && !camera->getGroupId().isEmpty())
        QnClientCameraResource::setAuthToCameraGroup(camera, authenticator);
    else
        camera->setAuth(authenticator);
}

void setCredentials(const State::Credentials& value, const Cameras& cameras)
{
    NX_ASSERT(value.login.hasValue() || value.password.hasValue());

    QAuthenticator authenticator;
    if (value.login.hasValue())
        authenticator.setUser(value.login());
    if (value.password.hasValue())
        authenticator.setPassword(value.password());

    if (!value.login.hasValue())
    {
        // Change only password, fetch logins from cameras.
        for (const auto& camera: cameras)
        {
            const auto oldAuth = camera->getAuth();
            if (oldAuth.password() == authenticator.password())
                continue;

            authenticator.setUser(oldAuth.user());
            setCredentials(camera, authenticator);
        }
    }
    else if (!value.password.hasValue())
    {
        // Change only login, fetch passwords from cameras.
        for (const auto& camera: cameras)
        {
            const auto oldAuth = camera->getAuth();
            if (oldAuth.user() == authenticator.user())
                continue;

            authenticator.setPassword(oldAuth.password());
            setCredentials(camera, authenticator);
        }
    }
    else
    {
        // Change both login and password.
        for (const auto& camera: cameras)
        {
            if (camera->getAuth() != authenticator)
                setCredentials(camera, authenticator);
        }
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

void setCustomMediaPort(int value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setMediaPort(value);
}

} // namespace

void CameraSettingsDialogStateConversionFunctions::applyStateToCameras(
    const State& state, const Cameras& cameras)
{
    if (state.isSingleCamera())
    {
        auto camera = cameras.first();
        camera->setName(state.singleCameraProperties.name());

        camera->setDewarpingParams(state.singleCameraSettings.fisheyeDewarping());
        camera->setLogicalId(state.singleCameraSettings.logicalId());

        if (state.devicesDescription.hasMotion == State::CombinedValue::All)
        {
            camera->setMotionType(state.singleCameraSettings.enableMotionDetection()
                ? camera->getDefaultMotionType()
                : Qn::MotionType::MT_NoMotion);

            camera->setMotionRegionList(state.singleCameraSettings.motionRegionList());
        }

        if (camera->isIOModule() && state.devicesDescription.isIoModule == State::CombinedValue::All)
        {
            camera->setProperty(ResourcePropertyKey::kIoOverlayStyle, QnLexical::serialized(
                state.singleIoModuleSettings.visualStyle()));

            const auto ioPortDataList = state.singleIoModuleSettings.ioPortsData();
            if (!ioPortDataList.empty()) //< Can happen if it's just discovered unauthorized module.
                camera->setIoPortDescriptions(ioPortDataList, /*needMerge*/ false);
        }

        if (state.singleCameraProperties.editableStreamUrls)
        {
            camera->updateSourceUrl(state.singleCameraSettings.primaryStream(),
                Qn::CR_LiveVideo, /*save*/ false);
            camera->updateSourceUrl(state.singleCameraSettings.secondaryStream(),
                Qn::CR_SecondaryLiveVideo, /*save*/ false);
        }

        camera->setUserEnabledAnalyticsEngines(state.analytics.enabledEngines());
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

    if ((state.credentials.login.hasValue() || state.credentials.password.hasValue())
        && state.devicesDescription.isWearable == State::CombinedValue::None)
    {
        setCredentials(state.credentials, cameras);
    }

    setRecordingDays(state.recording.minDays, state.recording.maxDays, cameras);

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

    if (state.audioEnabled.hasValue())
        setAudioEnabled(state.audioEnabled(), cameras);

    if (state.settingsOptimizationEnabled)
    {
        if (state.expert.dualStreamingDisabled.hasValue())
            setDualStreamingDisabled(state.expert.dualStreamingDisabled(), cameras);

        if (state.expert.useBitratePerGOP.hasValue()
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

    if (state.canSwitchPtzPresetTypes() && state.expert.preferredPtzPresetType.hasValue())
        setPreferredPtzPresetType(state.expert.preferredPtzPresetType(), cameras);

    if (state.canForcePtzCapabilities())
    {
        setForcedPtzCapabilities(
            state.expert.forcedPtzPanTiltCapability, state.expert.forcedPtzZoomCapability, cameras);
    }

    if (state.expert.rtpTransportType.hasValue())
        setRtpTransportType(state.expert.rtpTransportType(), cameras);

    if (state.expert.customMediaPort.hasValue())
        setCustomMediaPort(state.expert.customMediaPort(), cameras);

    if (state.expert.trustCameraTime.hasValue())
        setTrustCameraTime(state.expert.trustCameraTime(), cameras);
}

} // namespace nx::vms::client::desktop
