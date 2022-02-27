// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QList>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_preset.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/motion_window.h>
#include <nx/core/resource/using_media2_type.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/client/core/utils/rotation.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_types.h>
#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>
#include <nx/vms/client/desktop/common/flux/flux_types.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_data.h>
#include <nx/vms/client/desktop/utils/virtual_camera_state.h>
#include <utils/common/aspect_ratio.h>

#include "../data/recording_days.h"
#include "../data/analytics_engine_info.h"
#include "../camera_settings_tab.h"

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogState: AbstractFluxState
{
    using StreamIndex = nx::vms::api::StreamIndex;

    enum class RecordingHint
    {
        // Brush was changed (mode, fps, quality).
        brushChanged,

        // Recording was enabled, while schedule is empty.
        emptySchedule
    };

    enum class RecordingAlert
    {
        // High minimal archive length value selected.
        highArchiveLength,

        // High pre-recording value selected, resulting in increased RAM usage on the server.
        highPreRecordingValue,
    };

    enum class MotionHint
    {
        // Selected sensitivity was changed.
        sensitivityChanged,

        // Motion detection was enabled but the entire frame area is masked out.
        completelyMaskedOut
    };

    enum class MotionAlert
    {
        // Selection attempt produced too many motion rectangles.
        motionDetectionTooManyRectangles,

        // Selection attempt produced too many motion mask rectangles.
        motionDetectionTooManyMaskRectangles,

        // Selection attempt produced too many motion sensitivity rectangles.
        motionDetectionTooManySensitivityRectangles,
    };

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(ScheduleAlert,
        noMotion = 1 << 0,
        noObjects = 1 << 1,
        noBoth = 1 << 2,
        noMotionLowRes = 1 << 3,
        noObjectsLowRes = 1 << 4,
        noBothLowRes = 1 << 5
    );
    Q_DECLARE_FLAGS(ScheduleAlerts, ScheduleAlert)

    enum class ExpertAlert
    {
        // Quality and FPS settings in the Recording Schedule will become irrelevant.
        cameraControlYielded = 0x1
    };
    Q_DECLARE_FLAGS(ExpertAlerts, ExpertAlert)

    enum class MetadataRadioButton
    {
        motion,
        objects,
        both
    };

    bool hasChanges = false;
    bool readOnly = true;
    bool settingsOptimizationEnabled = false;
    GlobalPermissions globalPermissions;
    CameraSettingsTab selectedTab = CameraSettingsTab::general;

    // Generic cameras info.

    int devicesCount = 0;
    QnCameraDeviceType deviceType = QnCameraDeviceType::Mixed;

    struct MotionConstraints
    {
        int maxTotalRects = 0;
        int maxSensitiveRects = 0;
        int maxMaskRects = 0;
    };

    struct SingleCameraProperties
    {
        QnUuid id;
        UserEditable<QString> name;
        QString firmware;
        QString model;
        QString vendor;
        QString macAddress;
        QString ipAddress;
        QString webPageLabelText;
        QString settingsUrl;
        int overrideXmlHttpRequestTimeout = 0;
        QString overrideHttpUserAgent;
        bool isOnline = false;
        bool fixupRequestUrls = false;
        bool hasVideo = true;
        bool editableStreamUrls = false;
        bool networkLink = false;
        bool usbDevice = false;

        int maxFpsWithoutMotion = 0;

        QSize primaryStreamResolution;
        QSize secondaryStreamResolution;

        std::optional<QnCameraAdvancedParams> advancedSettingsManifest;

        /** Camera's supported object types, not filtered by engines. */
        std::map<QnUuid, std::set<QString>> supportedAnalyicsObjectTypes;
    };
    SingleCameraProperties singleCameraProperties;

    VirtualCameraState singleVirtualCameraState;
    QString virtualCameraUploaderName; //< Name of user currently uploading footage to virtual camera.

    struct SingleCameraSettings
    {
        UserEditable<nx::vms::api::dewarping::MediaData> dewarpingParams;

        UserEditable<QString> primaryStream;
        UserEditable<QString> secondaryStream;

        UserEditable<int> logicalId;

        QnUuid audioInputDeviceId;
        QnUuid audioOutputDeviceId;

        QStringList sameLogicalIdCameraNames; //< Read-only informational value.
    };
    SingleCameraSettings singleCameraSettings;

    enum class MotionStreamAlert
    {
        resolutionTooHigh,
        implicitlyDisabled,
        willBeImplicitlyDisabled
    };

    struct Motion
    {
        bool supportsSoftwareDetection = false;
        std::optional<MotionConstraints> constraints;

        std::optional<MotionStreamAlert> streamAlert;

        UserEditableMultiple<bool> enabled;
        int currentSensitivity = 0;

        UserEditable<QList<QnMotionRegion>> regionList;
        UserEditable<StreamIndex> stream;
        UserEditable<bool> forced;

        CombinedValue dependingOnDualStreaming = CombinedValue::All;
    };
    Motion motion;

    StreamIndex effectiveMotionStream() const
    {
        if (!isSingleCamera())
            return StreamIndex::undefined;

        return expert.dualStreamingDisabled()
            ? StreamIndex::primary
            : motion.stream();
    }

    bool isMotionImplicitlyDisabled() const
    {
        return motion.streamAlert == MotionStreamAlert::willBeImplicitlyDisabled
            || motion.streamAlert == MotionStreamAlert::implicitlyDisabled;
    }

    struct IoModuleSettings
    {
        UserEditable<QnIOPortDataList> ioPortsData;
        UserEditable<nx::vms::api::IoModuleVisualStyle> visualStyle;
    };
    IoModuleSettings singleIoModuleSettings;

    struct CombinedProperties
    {
        CombinedValue isDtsBased = CombinedValue::None;
        CombinedValue isVirtualCamera = CombinedValue::None;
        CombinedValue isIoModule = CombinedValue::None;
        CombinedValue isArecontCamera = CombinedValue::None;
        CombinedValue supportsAudio = CombinedValue::None;
        CombinedValue supportsVideo = CombinedValue::None;
        CombinedValue isAudioForced = CombinedValue::None;
        CombinedValue supportsAudioOutput = CombinedValue::None;
        CombinedValue hasMotion = CombinedValue::None;
        CombinedValue hasObjectDetection = CombinedValue::None;
        CombinedValue hasDualStreamingCapability = CombinedValue::None;
        CombinedValue hasRemoteArchiveCapability = CombinedValue::None;
        CombinedValue canSwitchPtzPresetTypes = CombinedValue::None;
        CombinedValue canForcePanTiltCapabilities = CombinedValue::None;
        CombinedValue canForceZoomCapability = CombinedValue::None;
        CombinedValue canAdjustPtzSensitivity = CombinedValue::None;
        CombinedValue hasCustomMediaPortCapability = CombinedValue::None;
        CombinedValue hasCustomMediaPort = CombinedValue::None;
        CombinedValue supportsSchedule = CombinedValue::None;
        CombinedValue isUdpMulticastTransportAllowed = CombinedValue::None;

        int maxFps = 0;
        int maxDualStreamingFps = 0;
    };
    CombinedProperties devicesDescription;

    struct Credentials
    {
        UserEditableMultiple<QString> login;
        UserEditableMultiple<QString> password;
    };
    Credentials credentials;

    struct PtzSensitivity
    {
        UserEditableMultiple<bool> separate;
        UserEditableMultiple<qreal> pan;
        UserEditableMultiple<qreal> tilt;
    };

    struct ExpertSettings
    {
        UserEditableMultiple<bool> dualStreamingDisabled;
        UserEditableMultiple<bool> cameraControlDisabled;
        UserEditableMultiple<bool> keepCameraTimeSettings;
        UserEditableMultiple<bool> useBitratePerGOP;
        UserEditableMultiple<nx::core::resource::UsingOnvifMedia2Type> useMedia2ToFetchProfiles;
        UserEditableMultiple<bool> primaryRecordingDisabled;
        UserEditableMultiple<bool> secondaryRecordingDisabled;
        UserEditableMultiple<nx::core::ptz::PresetType> preferredPtzPresetType;
        UserEditableMultiple<bool> forcedPtzPanTiltCapability;
        UserEditableMultiple<bool> forcedPtzZoomCapability;
        UserEditableMultiple<vms::api::RtpTransportType> rtpTransportType;
        UserEditableMultiple<bool> remoteMotionDetectionEnabled;
        PtzSensitivity ptzSensitivity;

        static constexpr int kDefaultRtspPort = 554;
        UserEditableMultiple<int> customMediaPort;
        int customMediaPortDisplayValue = kDefaultRtspPort;
        bool areOnvifSettingsApplicable = false;
        UserEditableMultiple<bool> trustCameraTime;
    };
    ExpertSettings expert;
    bool isDefaultExpertSettings = false;

    struct RecordingSettings
    {
        UserEditableMultiple<bool> enabled;

        RecordScheduleCellData brush;
        MetadataRadioButton metadataRadioButton = MetadataRadioButton::motion;

        media::CameraStreamCapability mediaStreamCapability;
        bool useBitratePerGop = false;
        QSize defaultStreamResolution;

        /** Some cameras do not allow to setup parameters (fps, quality). */
        bool parametersAvailable = true;
        bool customBitrateAvailable = false;
        bool customBitrateVisible = false;

        float minBitrateMbps = 0.0;
        float maxBitrateMpbs = 0.0;

        /** Value to be displayed in the dialog. */
        float bitrateMbps = 0.0;

        Qn::StreamQuality minRelevantQuality = Qn::StreamQuality::undefined;

        struct Thresholds
        {
            UserEditableMultiple<int> beforeSec;
            UserEditableMultiple<int> afterSec;
        };
        Thresholds thresholds;

        using ScheduleTasks = QnScheduleTaskList;
        UserEditableMultiple<ScheduleTasks> schedule;

        bool showQuality = true;
        bool showFps = true;

        RecordingDays minDays = RecordingDays::minDays();
        RecordingDays maxDays = RecordingDays::maxDays();

        float normalizedCustomBitrateMbps() const
        {
            const auto spread = maxBitrateMpbs - minBitrateMbps;
            if (qFuzzyIsNull(spread))
                return minBitrateMbps;
            return (bitrateMbps - minBitrateMbps) / spread;
        }
    };
    RecordingSettings recording;

    bool systemHasDevicesWithAudioInput = false;
    bool systemHasDevicesWithAudioOutput = false;

    UserEditableMultiple<bool> audioEnabled;
    UserEditableMultiple<bool> twoWayAudioEnabled;

    std::optional<RecordingHint> recordingHint;
    std::optional<RecordingAlert> recordingAlert;
    std::optional<MotionHint> motionHint;
    std::optional<MotionAlert> motionAlert;
    ScheduleAlerts scheduleAlerts;
    ExpertAlerts expertAlerts;

    struct ImageControlSettings
    {
        bool aspectRatioAvailable = true;
        UserEditableMultiple<QnAspectRatio> aspectRatio;
        bool rotationAvailable = true;
        UserEditableMultiple<nx::vms::client::core::StandardRotation> rotation;
    };
    ImageControlSettings imageControl;

    struct AnalyticsSettings
    {
        // Engines, actual for the selected single camera.
        QList<AnalyticsEngineInfo> engines;

        // Engines, which are enabled by the user.
        UserEditable<QSet<QnUuid>> userEnabledEngines;

        // All Engines which are actually enabled for the Camera. Includes device-dependent Engines.
        QSet<QnUuid> enabledEngines() const
        {
            QSet<QnUuid> result;
            for (const auto& engine: engines)
            {
                if (engine.isDeviceDependent || userEnabledEngines().contains(engine.id))
                    result.insert(engine.id);
            }
            return result;
        }

        struct EngineSettings
        {
            QJsonObject model;
            UserEditable<QJsonObject> values;
            bool loading = false;
        };
        QHash<QnUuid /*engineId*/, EngineSettings> settingsByEngineId;

        QnUuid currentEngineId;

        // This dictionary may contain engine ids that are no longer valid.
        // Don't iterate through all 'streamByEngineId' kv-pairs, iterate through 'engines' instead
        // and use 'engine.id' as a key. StreamIndex::undefined means there should be no selection.
        QHash<QnUuid /*engineId*/, UserEditable<StreamIndex> /*streamIndex*/> streamByEngineId;
    };
    AnalyticsSettings analytics;

    struct VirtualCameraMotionDetection
    {
        UserEditableMultiple<bool> enabled;
        UserEditableMultiple<int> sensitivity;
    };
    VirtualCameraMotionDetection virtualCameraMotion;

    bool virtualCameraIgnoreTimeZone = false;

    // Helper methods.

    bool isSingleCamera() const { return devicesCount == 1; }

    bool isSingleVirtualCamera() const
    {
        return isSingleCamera() && devicesDescription.isVirtualCamera == CombinedValue::All;
    }

    QnUuid singleCameraId() const
    {
        return isSingleCamera() ? singleCameraProperties.id : QnUuid();
    }

    int maxRecordingBrushFps() const
    {
        if (isSingleCamera() && !isMotionDetectionEnabled() && !isObjectDetectionSupported())
            return singleCameraProperties.maxFpsWithoutMotion;

        return recording.brush.recordingType == Qn::RecordingType::metadataAndLowQuality
            ? devicesDescription.maxDualStreamingFps
            : devicesDescription.maxFps;
    }

    bool isMotionDetectionEnabled() const
    {
        return devicesDescription.hasMotion == CombinedValue::All && motion.enabled.equals(true);
    }

    /**
     * Checks if motion detection is enabled and actually can work (hardware, or not exceeding
     * resolution limit or is forced).
     */
    bool isMotionDetectionActive() const
    {
        return isMotionDetectionEnabled() && !isMotionImplicitlyDisabled();
    }

    bool isObjectDetectionSupported() const
    {
        return devicesDescription.hasObjectDetection == CombinedValue::All;
    }

    bool supportsRecordingByEvents() const
    {
        return isMotionDetectionEnabled() || isObjectDetectionSupported();
    }

    /** Metadata types for recording, based on radiobutton state. */
    static nx::vms::api::RecordingMetadataTypes radioButtonToMetadataTypes(
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

        NX_ASSERT("Unreachable");
        return RecordingMetadataType::motion;
    }

    bool isDualStreamingEnabled() const
    {
        return devicesDescription.hasDualStreamingCapability == CombinedValue::All
            && expert.dualStreamingDisabled.equals(false)
            && expert.secondaryRecordingDisabled.equals(false);
    }

    bool supportsSchedule() const
    {
        return devicesDescription.supportsSchedule == CombinedValue::All;
    }

    bool supportsVideoStreamControl() const
    {
        return devicesDescription.isVirtualCamera == CombinedValue::None
            && devicesDescription.isDtsBased == CombinedValue::None
            && devicesDescription.supportsVideo == CombinedValue::All;
    }

    bool analyticsStreamSelectionEnabled(const QnUuid& engineId) const
    {
        return analytics.streamByEngineId.value(engineId)() != StreamIndex::undefined;
    }

    bool canSwitchPtzPresetTypes() const
    {
        return devicesDescription.canSwitchPtzPresetTypes != CombinedValue::None;
    }

    bool canSwitchPtzPresetTypesAll() const
    {
        return devicesDescription.canSwitchPtzPresetTypes == CombinedValue::All;
    }

    bool autoExclusivePtzPresets() const
    {
        return isSystemPtzPreset() || isNativePtzPreset();
    }

    bool isSystemPtzPreset() const
    {
        return expert.preferredPtzPresetType.equals(nx::core::ptz::PresetType::system);
    }

    bool isNativePtzPreset() const
    {
        return canSwitchPtzPresetTypesAll()
            && expert.preferredPtzPresetType.equals(nx::core::ptz::PresetType::native);
    }

    bool canForcePanTiltCapabilities() const
    {
        return devicesDescription.canForcePanTiltCapabilities == CombinedValue::All;
    }

    bool canForceZoomCapability() const
    {
        return devicesDescription.canForceZoomCapability == CombinedValue::All;
    }

    bool canShowWebPage() const
    {
        return isSingleCamera()
            && !singleCameraProperties.networkLink
            && !singleCameraProperties.usbDevice
            && devicesDescription.isVirtualCamera == CombinedValue::None;
    }

    bool canAdjustPtzSensitivity() const
    {
        return devicesDescription.canAdjustPtzSensitivity != CombinedValue::None;
    }

    bool cameraControlEnabled() const
    {
        return settingsOptimizationEnabled && !expert.cameraControlDisabled.valueOr(true);
    }

    /**
     * Advanced settings should be displayed for a single camera when manifest is already loaded and
     * it is not empty. Most properties are available only when camera is online, but some cameras
     * may have 'Advanced' page when offline also (e.g. with 'Reboot' button).
     */
    bool canShowAdvancedPage() const
    {
        const auto& manifest = singleCameraProperties.advancedSettingsManifest;
        return isSingleCamera()
            && manifest
            && !manifest->groups.empty()
            && (singleCameraProperties.isOnline || manifest->hasItemsAvailableInOffline());
    }

    bool isPageVisible(CameraSettingsTab page) const
    {
        switch (page)
        {
            case CameraSettingsTab::general:
                return true;

            case CameraSettingsTab::recording:
                return supportsSchedule();

            case CameraSettingsTab::io:
                return isSingleCamera()
                    && devicesDescription.isVirtualCamera == CombinedValue::None
                    && devicesDescription.isIoModule == CombinedValue::All;

            case CameraSettingsTab::motion:
                return isSingleCamera()
                    && devicesDescription.isVirtualCamera == CombinedValue::None
                    && devicesDescription.isDtsBased == CombinedValue::None
                    && devicesDescription.supportsVideo == CombinedValue::All
                    && devicesDescription.hasMotion == CombinedValue::All;

            case CameraSettingsTab::dewarping:
                return isSingleCamera() && singleCameraProperties.hasVideo;

            case CameraSettingsTab::advanced:
                return canShowAdvancedPage();

            case CameraSettingsTab::web:
                return canShowWebPage();

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
};

inline std::ostream& operator<<(std::ostream& os, CameraSettingsDialogState::ScheduleAlerts value)
{
    return os << nx::reflect::toString(value);
}

} // namespace nx::vms::client::desktop
