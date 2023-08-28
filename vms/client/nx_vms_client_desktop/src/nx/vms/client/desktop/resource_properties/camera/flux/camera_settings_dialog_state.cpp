// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_dialog_state.h"

#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

bool CameraSettingsDialogState::isSingleCamera() const
{
    return devicesCount == 1;
}

bool CameraSettingsDialogState::isSingleVirtualCamera() const
{
    return isSingleCamera() && devicesDescription.isVirtualCamera == CombinedValue::All;
}

QnUuid CameraSettingsDialogState::singleCameraId() const
{
    return isSingleCamera() ? singleCameraProperties.id : QnUuid();
}

int CameraSettingsDialogState::maxRecordingBrushFps() const
{
    return devicesDescription.maxFps;
}

bool CameraSettingsDialogState::isMotionDetectionEnabled() const
{
    return devicesDescription.hasMotion == CombinedValue::All && motion.enabled.equals(true);
}

/**
 * Checks if motion detection is enabled and actually can work (hardware, or not exceeding
 * resolution limit or is forced).
 */
bool CameraSettingsDialogState::isMotionDetectionActive() const
{
    return isMotionDetectionEnabled() && !isMotionImplicitlyDisabled();
}

bool CameraSettingsDialogState::isObjectDetectionSupported() const
{
    return devicesDescription.hasObjectDetection == CombinedValue::All;
}

bool CameraSettingsDialogState::supportsRecordingByEvents() const
{
    return isMotionDetectionEnabled() || isObjectDetectionSupported();
}

/** Metadata types for recording, based on radiobutton state. */
nx::vms::api::RecordingMetadataTypes CameraSettingsDialogState::radioButtonToMetadataTypes(
    MetadataRadioButton value)
{
    using namespace nx::vms::api;

    switch (value)
    {
        case MetadataRadioButton::objects:
            return RecordingMetadataType::objects;

        case MetadataRadioButton::both:
            return {RecordingMetadataType::motion | RecordingMetadataType::objects};

        case MetadataRadioButton::motion:
            return RecordingMetadataType::motion;
    }

    NX_ASSERT(false, "Unreachable");
    return RecordingMetadataType::motion;
}

bool CameraSettingsDialogState::isDualStreamingEnabled() const
{
    return devicesDescription.hasDualStreamingCapability == CombinedValue::All
        && expert.dualStreamingDisabled.equals(false)
        && expert.secondaryRecordingDisabled.equals(false);
}

bool CameraSettingsDialogState::supportsSchedule() const
{
    return devicesDescription.supportsSchedule == CombinedValue::All;
}

bool CameraSettingsDialogState::supportsVideoStreamControl() const
{
    return devicesDescription.isVirtualCamera == CombinedValue::None
        && devicesDescription.isDtsBased == CombinedValue::None
        && devicesDescription.supportsVideo == CombinedValue::All;
}

bool CameraSettingsDialogState::analyticsStreamSelectionEnabled(const QnUuid& engineId) const
{
    return analytics.streamByEngineId.value(engineId)() != StreamIndex::undefined;
}

bool CameraSettingsDialogState::canSwitchPtzPresetTypes() const
{
    return devicesDescription.canSwitchPtzPresetTypes == CombinedValue::All;
}

bool CameraSettingsDialogState::canForcePanTiltCapabilities() const
{
    return devicesDescription.canForcePanTiltCapabilities == CombinedValue::All;
}

bool CameraSettingsDialogState::canForceZoomCapability() const
{
    return devicesDescription.canForceZoomCapability == CombinedValue::All;
}

bool CameraSettingsDialogState::canShowWebPage() const
{
    return isSingleCamera() && devicesDescription.supportsWebPage == CombinedValue::All;
}

bool CameraSettingsDialogState::canAdjustPtzSensitivity() const
{
    return devicesDescription.canAdjustPtzSensitivity != CombinedValue::None;
}

bool CameraSettingsDialogState::cameraControlEnabled() const
{
    return settingsOptimizationEnabled && !expert.cameraControlDisabled.valueOr(true);
}

bool CameraSettingsDialogState::canShowAdvancedPage() const
{
    const auto& manifest = singleCameraProperties.advancedSettingsManifest;
    return isSingleCamera() && manifest && !manifest->groups.empty()
        && (singleCameraProperties.isOnline || manifest->hasItemsAvailableInOffline());
}

bool CameraSettingsDialogState::isPageVisible(CameraSettingsTab page) const
{
    switch (page)
    {
        case CameraSettingsTab::general:
            return true;

        case CameraSettingsTab::recording:
            return supportsSchedule();

        case CameraSettingsTab::io:
            return isSingleCamera() && devicesDescription.isVirtualCamera == CombinedValue::None
                && devicesDescription.isIoModule == CombinedValue::All;

        case CameraSettingsTab::motion:
            return isSingleCamera() && devicesDescription.isVirtualCamera == CombinedValue::None
                && devicesDescription.isDtsBased == CombinedValue::None
                && devicesDescription.supportsVideo == CombinedValue::All
                && devicesDescription.hasMotion == CombinedValue::All;

        case CameraSettingsTab::dewarping:
            return isSingleCamera() && singleCameraProperties.hasVideo;

        case CameraSettingsTab::hotspots:
            return isSingleCamera()
                && singleCameraProperties.supportsCameraHotspots
                && hasEditAccessRightsForAllCameras
                && ini().enableCameraHotspotsFeature;

        case CameraSettingsTab::advanced:
            return canShowAdvancedPage();

        case CameraSettingsTab::web:
            return canShowWebPage() && singleCameraProperties.hasViewLivePermission;

        case CameraSettingsTab::analytics:
            return isSingleCamera() && !analytics.engines.empty();

        // Always displaying for single camera as it contains Logical Id setup.
        case CameraSettingsTab::expert:
            return supportsVideoStreamControl() || isSingleCamera();

        default:
            NX_ASSERT(false, "Should never be here");
            return true;
    }
}

CameraSettingsDialogState::StreamIndex CameraSettingsDialogState::effectiveMotionStream() const
{
    if (!isSingleCamera())
        return StreamIndex::undefined;

    return expert.dualStreamingDisabled()
        ? StreamIndex::primary
        : motion.stream();
}

bool CameraSettingsDialogState::isMotionImplicitlyDisabled() const
{
    return motion.streamAlert == MotionStreamAlert::willBeImplicitlyDisabled
        || motion.streamAlert == MotionStreamAlert::implicitlyDisabled;
}

} // namespace nx::vms::client::desktop
