#include "camera_settings_dialog_state_conversion_functions.h"
#include "../redux/camera_settings_dialog_state.h"

#include <core/ptz/ptz_preset.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>

#include <nx/client/core/motion/motion_grid.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/range_adapters.h>
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
    for (const auto& camera: cameras)
    {
        camera->setMinDays(minDays.automatic() ? -minDays.value() : minDays.value());
        camera->setMaxDays(maxDays.automatic() ? -maxDays.value() : maxDays.value());
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
    for (const auto& camera: cameras)
    {
        if (camera->isDtsBased())
            continue;

        camera->setScheduleTasks(schedule);
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
        if (camera->ptzCapabilitiesUserIsAllowedToModify() == Ptz::Capability::NoPtzCapabilities)
            continue;

        auto capabilities = camera->ptzCapabilitiesAddedByUser();
        if (panTilt)
            capabilities.setFlag(Ptz::ContinuousPanTiltCapabilities, *panTilt);
        if (zoom)
            capabilities.setFlag(Ptz::ContinuousZoomCapability, *zoom);

        camera->setPtzCapabilitiesAddedByUser(capabilities);
    }
}

void setRtpTransportType(nx::vms::api::RtpTransportType value, const Cameras& cameras)
{
    const auto valueStr = value == nx::vms::api::RtpTransportType::automatic
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

void setForcedMotionStreamType(nx::vms::api::StreamIndex value, const Cameras& cameras)
{
    const bool isMotionDetectionForced = (value != nx::vms::api::StreamIndex::undefined);
    const QnSecurityCamResource::MotionStreamIndex index{value, isMotionDetectionForced};
    for (const auto& camera: cameras)
        camera->setMotionStreamIndex(index);
}

void setWearableMotionEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->hasFlags(Qn::wearable_camera))
            camera->setRemoteArchiveMotionDetectionEnabled(value);
    }
}

void setRemoteArchiveMotionDetection(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->hasCameraCapabilities(Qn::RemoteArchiveCapability))
            camera->setRemoteArchiveMotionDetectionEnabled(value);
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

bool fixupSchedule(QnScheduleTaskList& schedule, bool motionSupported, bool motionPlusLQSupported)
{
    if (motionSupported && motionPlusLQSupported)
        return false;

    int changed = schedule.size();
    for (auto& task: schedule)
    {
        if (task.recordingType == Qn::RecordingType::motionAndLow && !motionPlusLQSupported)
            task.recordingType = Qn::RecordingType::always;
        else if (task.recordingType == Qn::RecordingType::motionOnly && !motionSupported)
            task.recordingType = Qn::RecordingType::always;
        else
            --changed;
    }

    return changed > 0;
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

        if (state.devicesDescription.hasMotion == CombinedValue::All)
        {
            camera->setMotionType(state.enableMotionDetection()
                ? camera->getDefaultMotionType()
                : Qn::MotionType::MT_NoMotion);

            camera->setMotionRegionList(state.singleCameraSettings.motionRegionList());
        }

        if (camera->isIOModule() && state.devicesDescription.isIoModule == CombinedValue::All)
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

        for (const auto& [id, index]: nx::utils::constKeyValueRange(state.analytics.streamByEngineId))
        {
            if (index.hasUser())
                camera->setAnalyzedStreamIndex(id, index());
        }
    }

    if (state.devicesDescription.isWearable == CombinedValue::All)
    {
        if (state.wearableMotion.enabled.hasValue())
        {
            setWearableMotionEnabled(state.wearableMotion.enabled(), cameras);

            if (state.wearableMotion.enabled() && state.wearableMotion.sensitivity.hasValue())
                setWearableMotionSensitivity(state.wearableMotion.sensitivity(), cameras);
        }
    }

    if ((state.credentials.login.hasValue() || state.credentials.password.hasValue())
        && state.devicesDescription.isWearable == CombinedValue::None)
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

    const bool motionSupported = state.isMotionDetectionEnabled();
    const bool motionPlusLQSupported = state.supportsMotionPlusLQ();

    if (state.recording.schedule.hasValue())
    {
        auto schedule = state.recording.schedule();
        if (!motionSupported || !motionPlusLQSupported)
            fixupSchedule(schedule, motionSupported, motionPlusLQSupported);

        setSchedule(schedule, cameras);
    }
    else if (!motionSupported || !motionPlusLQSupported)
    {
        for (const auto& camera: cameras)
        {
            if (camera->isDtsBased())
                continue;

            auto schedule = camera->getScheduleTasks();
            if (fixupSchedule(schedule, motionSupported, motionPlusLQSupported))
                setSchedule(schedule, {camera});
        }
    }

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

        if (state.devicesDescription.isArecontCamera == CombinedValue::None
            && state.expert.cameraControlDisabled.hasValue())
        {
            setCameraControlDisabled(state.expert.cameraControlDisabled(), cameras);
        }
    }

    if (state.expert.primaryRecordingDisabled.hasValue())
        setPrimaryRecordingDisabled(state.expert.primaryRecordingDisabled(), cameras);

    if (state.expert.secondaryRecordingDisabled.hasValue())
        setSecondaryRecordingDisabled(state.expert.secondaryRecordingDisabled(), cameras);

    if (state.devicesDescription.supportsMotionStreamOverride == CombinedValue::All
        && state.expert.forcedMotionStreamType.hasValue())
    {
        setForcedMotionStreamType(state.expert.forcedMotionStreamType(), cameras);
    }

    if (state.devicesDescription.hasRemoteArchiveCapability == CombinedValue::All
        && state.expert.remoteMotionDetectionEnabled.hasValue())
    {
        setRemoteArchiveMotionDetection(state.expert.remoteMotionDetectionEnabled(), cameras);
    }

    if (state.canSwitchPtzPresetTypes() && state.expert.preferredPtzPresetType.hasValue())
        setPreferredPtzPresetType(state.expert.preferredPtzPresetType(), cameras);

    if (state.canForcePanTiltCapabilities() || state.canForceZoomCapability())
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
