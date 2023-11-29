// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_dialog_state_conversion_functions.h"

#include <QtWidgets/QApplication>

#include <core/ptz/ptz_preset.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/resource_display_info.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/client/core/motion/motion_grid.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

namespace {

using State = CameraSettingsDialogState;
using Cameras = QnVirtualCameraResourceList;
using Rotation = core::Rotation;

using StreamIndex = nx::vms::api::StreamIndex;
using MotionStreamIndex = QnSecurityCamResource::MotionStreamIndex;

enum class PeriodType
{
    minPeriod,
    maxPeriod
};

void tryStoreRecordingPeriodChanges(
    const RecordingPeriod& data,
    const QnVirtualCameraResourceList& cameras,
    PeriodType type)
{
    using getterType = std::function<std::chrono::seconds(const QnVirtualCameraResourcePtr&)>;
    using setterType = std::function<void(const QnVirtualCameraResourcePtr&, std::chrono::seconds)>;

    static const getterType minGetter =
        [](const QnVirtualCameraResourcePtr& camera) { return camera->minPeriod(); };
    static const setterType minSetter =
        [](const QnVirtualCameraResourcePtr& camera, std::chrono::seconds value)
        {
            return camera->setMinPeriod(value);
        };

    static const getterType maxGetter =
        [](const QnVirtualCameraResourcePtr& camera) { return camera->maxPeriod(); };
    static const setterType maxSetter =
        [](const QnVirtualCameraResourcePtr& camera, std::chrono::seconds value)
        {
            return camera->setMaxPeriod(value);
        };

    const setterType& setter = type == PeriodType::minPeriod ? minSetter : maxSetter;

    if (!data.isApplicable())
        return;

    if (data.hasManualPeriodValue())
    {
        // Sets direct values.
        for (const auto& camera: cameras)
            setter(camera, data.rawValue());

        return;
    }

    // Preserves days value if there are different ones.

    const getterType& getter = type == PeriodType::minPeriod ? minGetter : maxGetter;
    for (const auto& camera: cameras)
    {
        const auto value = getter(camera);
        const bool currentManualMode = value.count() >= 0;
        if (currentManualMode != data.isManualMode())
            setter(camera, -value);
    }
}

//TODO #lbusygin: move conversion to resource layer
QString boolToPropertyStr(bool value)
{
    return value ? lit("1") : lit("0");
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

        const auto rotation = qRound(value.value());
        camera->setForcedRotation(rotation == 0 ? std::optional<int>{} : rotation);
    }
}

void setCustomAspectRatio(
    const QnAspectRatio& value,
    const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        NX_ASSERT(camera->hasVideo() && !camera->hasFlags(Qn::virtual_camera));
        if (camera->hasVideo() && !camera->hasFlags(Qn::virtual_camera))
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
        camera->setScheduleEnabled(value);
}

void setDualStreamingDisabled(bool value, bool forceMotion, const Cameras& cameras)
{
    using Reducer = CameraSettingsDialogStateReducer;
    forceMotion = forceMotion && value; //< Force only if dual streaming is being disabled.

    for (const auto& camera: cameras)
    {
        if (!camera->hasDualStreamingInternal())
            continue;

        if (forceMotion && Reducer::isMotionDetectionDependingOnDualStreaming(camera))
            camera->setMotionStreamIndex({StreamIndex::primary, true});

        camera->setDisableDualStreaming(value);
    }
}

void setCameraControlDisabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->isVideoQualityAdjustable())
            camera->setCameraControlDisabled(value);
    }
}

void setKeepCameraTimeSettings(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setKeepCameraTimeSettings(value);
}

void setUseBitratePerGOP(bool value, const Cameras& cameras)
{
    const auto valueStr = boolToPropertyStr(value);
    for (const auto& camera: cameras)
        camera->setProperty(ResourcePropertyKey::kBitratePerGOP, valueStr);
}

void setUseMedia2ToFetchProfiles(nx::core::resource::UsingOnvifMedia2Type value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        const auto previousValue = nx::reflect::fromString(
            camera->getProperty(ResourcePropertyKey::kUseMedia2ToFetchProfiles).toStdString(),
            nx::core::resource::UsingOnvifMedia2Type::autoSelect);

        camera->setProperty(ResourcePropertyKey::kUseMedia2ToFetchProfiles,
            QString::fromStdString(nx::reflect::toString(value)));

        if (previousValue != value)
        {
            // Force server to reinitialize the device.
            camera->setProperty(QnVirtualCameraResource::kUsingOnvifMedia2Type,
                QString::fromStdString(nx::reflect::toString(value)));
        }
    }
}

void setPrimaryRecordingDisabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setPrimaryStreamRecorded(!value);
}

void setSecondaryRecordingDisabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setSecondaryStreamRecorded(!value);
}

void setRecordAudioEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setRecordAudio(value);
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

void setPtzSensitivities(const State::PtzSensitivity& value, const Cameras& cameras)
{
    if (!value.separate.hasValue())
        return;

    constexpr qreal kPtzSameTiltSensitivityAsPan = 0.0;

    const std::optional<qreal> pan = value.pan;
    const std::optional<qreal> tilt = value.separate() ? value.tilt : std::optional<qreal>(0.0);

    if (!pan && !tilt)
        return;

    const bool partial = !pan || !tilt;

    for (const auto& camera: cameras)
    {
        if (!camera->hasAnyOfPtzCapabilities(Ptz::ContinuousPanTiltCapabilities))
            continue;

        QPointF sensitivity = partial ? camera->ptzPanTiltSensitivity() : QPointF();
        if (pan)
            sensitivity.setX(*pan);
        if (tilt)
            sensitivity.setY(*tilt);

        if (sensitivity.y() > 0.0)
            camera->setPtzPanTiltSensitivity(sensitivity);
        else
            camera->setPtzPanTiltSensitivity(sensitivity.x());
    }
}

void setRtpTransportType(nx::vms::api::RtpTransportType value, const Cameras& cameras)
{
    const auto valueStr = value == nx::vms::api::RtpTransportType::automatic
        ? QString()
        : QString::fromStdString(nx::reflect::toString(value));

    for (const auto& camera: cameras)
        camera->setProperty(QnMediaResource::rtpTransportKey(), valueStr);
}

void setForcedPrimaryProfile(const QString& value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setForcedProfile(value, nx::vms::api::StreamIndex::primary);
}

void setForcedSecondaryProfile(const QString& value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setForcedProfile(value, nx::vms::api::StreamIndex::secondary);
}

void setRemoteArchiveAutoImportEnabled(const bool& value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setRemoteArchiveSynchronizationEnabled(value);
}

void setTrustCameraTime(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setTrustCameraTime(value);
}

void applyMotionDetectionParameters(
    const State& state, const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera))
        return;

    const auto motionType = state.motion.enabled()
        ? camera->getDefaultMotionType()
        : nx::vms::api::MotionType::none;

    const bool forceMotionDetection =
        state.motion.streamAlert == State::MotionStreamAlert::resolutionTooHigh;

    camera->setMotionType(motionType);
    camera->setMotionRegionList(state.motion.regionList());

    if (!state.motion.supportsSoftwareDetection)
        return;

    camera->setMotionStreamIndex({state.effectiveMotionStream(), forceMotionDetection});
}

void setVirtualCameraMotionEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->hasFlags(Qn::virtual_camera))
            camera->setRemoteArchiveMotionDetectionEnabled(value);
    }
}

void setVirtualCameraIgnoreTimeZoneEnabled(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->hasFlags(Qn::virtual_camera))
            camera->setVirtualCameraIgnoreTimeZone(value);
    }
}

void setRemoteArchiveMotionDetection(bool value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
    {
        if (camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::remoteArchive))
            camera->setRemoteArchiveMotionDetectionEnabled(value);
    }
}

void setAudioEnabled(bool value, const Cameras& cameras)
{
    const bool multipleEnable = value && (cameras.size() > 1);
    QnVirtualCameraResourceList skippedCameras;

    for (const auto& camera: cameras)
    {
        if (multipleEnable && !camera->isAudioSupported() && !camera->isAudioEnabled())
        {
            skippedCameras.push_back(camera);
            continue;
        }

        // Enabled state of audio cannot be changed for devices with forced audio.
        if (!camera->isAudioForced())
            camera->setAudioEnabled(value);
        if (!value && !camera->audioInputDeviceId().isNull())
            camera->setAudioInputDeviceId(QnUuid());
    }

    if (!skippedCameras.empty())
    {
        QnMessageBox messageBox(QnMessageBoxIcon::Warning,
            CameraSettingsDialogStateConversionFunctions::tr(
                "Failed to enable audio on %n devices", "", skippedCameras.size()),
            CameraSettingsDialogStateConversionFunctions::tr(
                "These devices do not have audio inputs or are not configured correctly.",
                "", skippedCameras.size()),
            QDialogButtonBox::Ok, QDialogButtonBox::Ok, QApplication::activeWindow());
        messageBox.addCustomWidget(new QnResourceListView(skippedCameras, &messageBox));
        messageBox.exec();
    }
}

void setTwoWayAudioEnabled(bool value, const Cameras& cameras)
{
    const bool multipleEnable = value && (cameras.size() > 1);
    QnVirtualCameraResourceList skippedCameras;

    for (const auto& camera: cameras)
    {
        if (camera->hasFlags(Qn::virtual_camera))
            continue;

        if (multipleEnable && !camera->hasTwoWayAudio() && !camera->isTwoWayAudioEnabled())
        {
            skippedCameras.push_back(camera);
            continue;
        }

        camera->setTwoWayAudioEnabled(value);
        if (!value && !camera->audioOutputDeviceId().isNull())
            camera->setAudioOutputDeviceId(QnUuid());
    }

    if (!skippedCameras.empty())
    {
        QnMessageBox messageBox(QnMessageBoxIcon::Warning,
            CameraSettingsDialogStateConversionFunctions::tr(
                "Failed to enable 2-way audio on %n devices", "", skippedCameras.size()),
            CameraSettingsDialogStateConversionFunctions::tr(
                "These devices do not have audio outputs or are not configured correctly.",
                "", skippedCameras.size()),
            QDialogButtonBox::Ok, QDialogButtonBox::Ok, QApplication::activeWindow());
        messageBox.addCustomWidget(new QnResourceListView(skippedCameras, &messageBox));
        messageBox.exec();
    }
}

void setAudioInputDeviceId(const QnUuid& deviceId, const QnVirtualCameraResourcePtr& camera)
{
    if (camera->audioInputDeviceId() != deviceId)
        camera->setAudioInputDeviceId(deviceId);
}

void setAudioOutputDeviceId(const QnUuid& deviceId, const QnVirtualCameraResourcePtr& camera)
{
    if (camera->audioOutputDeviceId() != deviceId)
        camera->setAudioOutputDeviceId(deviceId);
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
    if (!NX_ASSERT(value.password.hasValue(), "New camera credentials password should be defined"))
        return;

    QAuthenticator authenticator;
    if (value.login.hasValue())
        authenticator.setUser(value.login());
    authenticator.setPassword(value.password());

    if (!value.login.hasValue())
    {
        // Change only password, fetch logins from cameras.
        for (const auto& camera: cameras)
        {
            const auto oldAuth = camera->getAuth();
            authenticator.setUser(oldAuth.user());
            setCredentials(camera, authenticator);
        }
    }
    else
    {
        // Change both login and password.
        for (const auto& camera: cameras)
            setCredentials(camera, authenticator);
    }
}

void setVirtualCameraMotionSensitivity(int value, const Cameras& cameras)
{
    QnMotionRegion region;
    region.addRect(value, QRect(0, 0, core::MotionGrid::kWidth, core::MotionGrid::kHeight));

    for (const auto& camera: cameras)
    {
        if (!camera->hasFlags(Qn::virtual_camera))
            continue;

        NX_ASSERT(camera->getVideoLayout()->channelCount() == 1);
        QList<QnMotionRegion> regionsList;
        regionsList.push_back(region);
        camera->setMotionRegionList(regionsList);
    }
}

void setCustomMediaPort(int value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setMediaPort(value);
}

void setCustomWebPagePort(int value, const Cameras& cameras)
{
    for (const auto& camera: cameras)
        camera->setCustomWebPagePort(value);
}

} // namespace

void CameraSettingsDialogStateConversionFunctions::applyStateToCameras(
    const State& state, const Cameras& cameras)
{
    if (state.isSingleCamera())
    {
        auto camera = cameras.first();
        camera->setName(state.singleCameraProperties.name());

        camera->setDewarpingParams(state.singleCameraSettings.dewarpingParams());
        camera->setLogicalId(state.singleCameraSettings.logicalId());

        if (state.devicesDescription.hasMotion != CombinedValue::None)
            applyMotionDetectionParameters(state, camera);

        if (camera->isIOModule() && state.devicesDescription.isIoModule == CombinedValue::All)
        {
            camera->setProperty(ResourcePropertyKey::kIoOverlayStyle,
                QString::fromStdString(
                    nx::reflect::toString(state.singleIoModuleSettings.visualStyle())));

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

        camera->setUserEnabledAnalyticsEngines(state.analytics.userEnabledEngines());

        for (const auto& [id, index]: nx::utils::constKeyValueRange(state.analytics.streamByEngineId))
        {
            if (index.hasUser() && NX_ASSERT(index() != nx::vms::api::StreamIndex::undefined))
                camera->setAnalyzedStreamIndex(id, index());
        }

        setAudioInputDeviceId(state.singleCameraSettings.audioInputDeviceId, camera);
        setAudioOutputDeviceId(state.singleCameraSettings.audioOutputDeviceId, camera);
    }

    if (state.devicesDescription.isVirtualCamera == CombinedValue::All)
    {
        if (state.virtualCameraMotion.enabled.hasValue())
        {
            setVirtualCameraMotionEnabled(state.virtualCameraMotion.enabled(), cameras);
            setVirtualCameraIgnoreTimeZoneEnabled(state.virtualCameraIgnoreTimeZone, cameras);

            if (state.virtualCameraMotion.enabled() && state.virtualCameraMotion.sensitivity.hasValue())
                setVirtualCameraMotionSensitivity(state.virtualCameraMotion.sensitivity(), cameras);
        }
    }

    if (state.credentials.password.hasValue()
        && state.devicesDescription.isVirtualCamera == CombinedValue::None)
    {
        setCredentials(state.credentials, cameras);
    }

    tryStoreRecordingPeriodChanges(state.recording.minPeriod, cameras, PeriodType::minPeriod);
    tryStoreRecordingPeriodChanges(state.recording.maxPeriod, cameras, PeriodType::maxPeriod);

    if (state.imageControl.aspectRatio.hasValue())
        setCustomAspectRatio(state.imageControl.aspectRatio(), cameras);

    if (state.imageControl.rotation.hasValue())
        setCustomRotation(Rotation{state.imageControl.rotation()}, cameras);

    if (state.recording.enabled.hasValue())
        setRecordingEnabled(state.recording.enabled(), cameras);

    if (state.recording.schedule.hasValue())
    {
        setSchedule(state.recording.schedule(), cameras);
    }
    else // Multiple camera edit, fill schedule with default value for cameras without own.
    {
        for (const auto& camera: cameras)
        {
            if (camera->isDtsBased())
                continue;

            // TODO: Code duplication with `calculateRecordingSchedule` method in the reducer.
            if (camera->getScheduleTasks().empty())
            {
                int maxFps = camera->getMaxFps();
                if (camera->getStatus() == nx::vms::api::ResourceStatus::unauthorized)
                    maxFps = QnSecurityCamResource::kDefaultMaxFps;

                setSchedule(defaultSchedule(maxFps), {camera});
            }
        }
    }

    if (state.recording.thresholds.beforeSec.hasValue())
        setRecordingBeforeThreshold(state.recording.thresholds.beforeSec(), cameras);

    if (state.recording.thresholds.afterSec.hasValue())
        setRecordingAfterThreshold(state.recording.thresholds.afterSec(), cameras);

    if (state.devicesDescription.isAudioForced == CombinedValue::None
        && state.audioEnabled.hasValue())
    {
        if (state.isSingleCamera() && state.audioEnabled()
            && state.singleCameraSettings.audioInputDeviceId.isNull()
            && !cameras.first()->isAudioSupported())
        {
            setAudioEnabled(false, cameras);

            const auto caption = tr("Audio will be disabled");
            const auto text = tr("You need to select a device that will provide audio.");
            QnMessageBox::warning(QApplication::activeWindow(), caption, text);
        }
        else
        {
            setAudioEnabled(state.audioEnabled(), cameras);
        }
    }

    if (state.twoWayAudioEnabled.hasValue())
    {
        if (state.isSingleCamera() && state.twoWayAudioEnabled()
            && state.singleCameraSettings.audioOutputDeviceId.isNull()
            && !cameras.first()->hasTwoWayAudio())
        {
            setTwoWayAudioEnabled(false, cameras);

            const auto caption = tr("2-way audio will be disabled");
            const auto text = tr("You need to select a device to transmit the audio stream to and "
                "use for audio playback.");
            QnMessageBox::warning(QApplication::activeWindow(), caption, text);
        }
        else
        {
            setTwoWayAudioEnabled(state.twoWayAudioEnabled(), cameras);
        }
    }

    if (state.settingsOptimizationEnabled)
    {
        if (state.expert.useBitratePerGOP.hasValue()
            && !state.expert.cameraControlDisabled.valueOr(false))
        {
            setUseBitratePerGOP(state.expert.useBitratePerGOP(), cameras);
        }

        if (state.devicesDescription.isArecontCamera == CombinedValue::None
            && state.recording.parametersAvailable
            && state.expert.cameraControlDisabled.hasValue())
        {
            setCameraControlDisabled(state.expert.cameraControlDisabled(), cameras);
        }

        if (state.expert.keepCameraTimeSettings.hasValue())
            setKeepCameraTimeSettings(state.expert.keepCameraTimeSettings(), cameras);
    }

    if (state.expert.dualStreamingDisabled.hasValue())
    {
        setDualStreamingDisabled(state.expert.dualStreamingDisabled(),
            /*need to force motion*/ !state.isSingleCamera() && state.motion.forced(),
            cameras);
    }

    if (state.expert.useMedia2ToFetchProfiles.hasValue())
        setUseMedia2ToFetchProfiles(state.expert.useMedia2ToFetchProfiles(), cameras);

    if (state.expert.primaryRecordingDisabled.hasValue())
        setPrimaryRecordingDisabled(state.expert.primaryRecordingDisabled(), cameras);

    if (state.expert.secondaryRecordingDisabled.hasValue())
        setSecondaryRecordingDisabled(state.expert.secondaryRecordingDisabled(), cameras);

    if (state.expert.recordAudioEnabled.hasValue())
        setRecordAudioEnabled(state.expert.recordAudioEnabled(), cameras);

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

    if (state.canAdjustPtzSensitivity())
        setPtzSensitivities(state.expert.ptzSensitivity, cameras);

    if (state.expert.rtpTransportType.hasValue())
        setRtpTransportType(state.expert.rtpTransportType(), cameras);

    if (state.expert.forcedPrimaryProfile.hasValue())
        setForcedPrimaryProfile(state.expert.forcedPrimaryProfile(), cameras);

    if (state.expert.forcedSecondaryProfile.hasValue())
        setForcedSecondaryProfile(state.expert.forcedSecondaryProfile(), cameras);

    if (state.expert.remoteArchiveAutoImportEnabled.hasValue())
    {
        setRemoteArchiveAutoImportEnabled(
            state.expert.remoteArchiveAutoImportEnabled(),
            cameras);
    }

    if (state.expert.customWebPagePort.hasValue())
        setCustomWebPagePort(state.expert.customWebPagePort(), cameras);

    if (state.expert.customMediaPort.hasValue())
        setCustomMediaPort(state.expert.customMediaPort(), cameras);

    if (state.expert.trustCameraTime.hasValue())
        setTrustCameraTime(state.expert.trustCameraTime(), cameras);
}

} // namespace nx::vms::client::desktop
