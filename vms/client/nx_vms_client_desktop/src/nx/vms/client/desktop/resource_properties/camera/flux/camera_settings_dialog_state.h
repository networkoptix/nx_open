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
#include <nx/reflect/instrument.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/device_profile.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/client/core/analytics/analytics_engine_info.h>
#include <nx/vms/client/core/analytics/analytics_settings_types.h>
#include <nx/vms/client/core/utils/rotation.h>
#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>
#include <nx/vms/client/desktop/common/flux/flux_types.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_data.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_state.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>
#include <nx/vms/common/resource/remote_archive_types.h>
#include <utils/common/aspect_ratio.h>

#include "../camera_settings_tab.h"
#include "../data/recording_period.h"

namespace nx::vms::client::desktop {

namespace camera_settings_detail {

struct SingleCameraProperties
{
    QnUuid id;
    UserEditable<QString> name;
    QString firmware;
    QString model;
    QString vendor;
    QString macAddress;
    QString ipAddress;
    nx::utils::Url baseCameraUrl;
    QString settingsUrlPath;
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
    bool supportsCameraHotspots = false;
    Qn::Permissions permissions = Qn::NoPermissions;

    QSize primaryStreamResolution;
    QSize secondaryStreamResolution;

    std::optional<QnCameraAdvancedParams> advancedSettingsManifest;

    /** Camera's supported object types, not filtered by engines. */
    std::map<QnUuid, std::set<QString>> supportedAnalyicsObjectTypes;
};

NX_REFLECTION_INSTRUMENT(SingleCameraProperties,
    (id)(name)(firmware)(model)(vendor)(macAddress)(ipAddress)(baseCameraUrl)(settingsUrlPath)
    (webPageLabelText)(settingsUrl)
    (overrideXmlHttpRequestTimeout)(overrideHttpUserAgent)(isOnline)(fixupRequestUrls)(hasVideo)
    (editableStreamUrls)(networkLink)(usbDevice)(permissions)
    (supportsCameraHotspots)(primaryStreamResolution)(secondaryStreamResolution))

struct CombinedProperties
{
    CombinedValue isDtsBased = CombinedValue::None;
    CombinedValue isVirtualCamera = CombinedValue::None;
    CombinedValue isIoModule = CombinedValue::None;
    CombinedValue isArecontCamera = CombinedValue::None;
    CombinedValue supportsAudio = CombinedValue::None;
    CombinedValue supportsVideo = CombinedValue::None;
    CombinedValue supportsWebPage = CombinedValue::None;
    CombinedValue isAudioForced = CombinedValue::None;
    CombinedValue supportsAudioOutput = CombinedValue::None;
    CombinedValue hasMotion = CombinedValue::None;
    CombinedValue hasObjectDetection = CombinedValue::None;
    CombinedValue hasDualStreamingCapability = CombinedValue::None;
    CombinedValue hasRemoteArchiveCapability = CombinedValue::None;
    CombinedValue canSwitchPtzPresetTypes = CombinedValue::None;
    CombinedValue canForcePanTiltCapabilities = CombinedValue::None;
    CombinedValue canForceZoomCapability = CombinedValue::None;
    CombinedValue hasPanTiltCapabilities = CombinedValue::None;
    CombinedValue canAdjustPtzSensitivity = CombinedValue::None;
    CombinedValue hasCustomMediaPortCapability = CombinedValue::None;
    CombinedValue hasCustomMediaPort = CombinedValue::None;
    CombinedValue supportsSchedule = CombinedValue::None;
    CombinedValue isUdpMulticastTransportAllowed = CombinedValue::None;
    CombinedValue streamCapabilitiesInitialized = CombinedValue::None;

    int maxFps = 0;
};
NX_REFLECTION_INSTRUMENT(CombinedProperties,
    (isDtsBased)(isVirtualCamera)(isIoModule)(isArecontCamera)(supportsAudio)(supportsVideo)
    (supportsWebPage)
    (isAudioForced)(supportsAudioOutput)(hasMotion)(hasObjectDetection)(hasDualStreamingCapability)
    (hasRemoteArchiveCapability)(canSwitchPtzPresetTypes)(canForcePanTiltCapabilities)
    (canForceZoomCapability)(hasPanTiltCapabilities)(canAdjustPtzSensitivity)
    (hasCustomMediaPortCapability)
    (hasCustomMediaPort)(supportsSchedule)(isUdpMulticastTransportAllowed)
    (streamCapabilitiesInitialized)(maxFps))

struct MotionConstraints
{
    int maxTotalRects = 0;
    int maxSensitiveRects = 0;
    int maxMaskRects = 0;
};
NX_REFLECTION_INSTRUMENT(MotionConstraints, (maxTotalRects)(maxSensitiveRects)(maxMaskRects))

NX_REFLECTION_ENUM_CLASS(MotionStreamAlert,
    resolutionTooHigh,
    implicitlyDisabled,
    willBeImplicitlyDisabled
);

struct Motion
{
    UserEditableMultiple<bool> enabled;

    bool supportsSoftwareDetection = false;
    std::optional<MotionConstraints> constraints;

    std::optional<MotionStreamAlert> streamAlert;

    int currentSensitivity = 0;

    UserEditable<QList<QnMotionRegion>> regionList;
    UserEditable<nx::vms::api::StreamIndex> stream;
    UserEditable<bool> forced;

    CombinedValue dependingOnDualStreaming = CombinedValue::All;
};
NX_REFLECTION_INSTRUMENT(Motion,
    (enabled)(supportsSoftwareDetection)(constraints)(streamAlert)(currentSensitivity)(stream)
    (forced)(dependingOnDualStreaming))

} // namespace camera_settings_detail

struct NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogState: AbstractFluxState
{
    using StreamIndex = nx::vms::api::StreamIndex;
    using MotionStreamAlert = camera_settings_detail::MotionStreamAlert;

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
        highArchiveLength = 1 << 0,

        // High pre-recording value selected, resulting in increased RAM usage on the server.
        highPreRecordingValue = 1 << 1,
    };
    Q_DECLARE_FLAGS(RecordingAlerts, RecordingAlert)

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

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(MetadataRadioButton,
        motion,
        objects,
        both
    );

    bool hasChanges = false;
    bool readOnly = true;
    bool allCamerasEditable = false;
    bool settingsOptimizationEnabled = false;
    bool hasPowerUserPermissions = false;
    bool hasEventLogPermission = false;
    bool hasEditAccessRightsForAllCameras = false;
    CameraSettingsTab selectedTab = CameraSettingsTab::general;

    // System info.

    bool saasInitialized = false;

    // Generic cameras info.

    int devicesCount = 0;
    QnCameraDeviceType deviceType = QnCameraDeviceType::Mixed;

    using SingleCameraProperties = camera_settings_detail::SingleCameraProperties;
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

        UserEditable<bool> cameraHotspotsEnabled;
        UserEditable<nx::vms::common::CameraHotspotDataList> cameraHotspots;

        QStringList sameLogicalIdCameraNames; //< Read-only informational value.
    };
    SingleCameraSettings singleCameraSettings;

    camera_settings_detail::Motion motion;

    struct IoModuleSettings
    {
        UserEditable<QnIOPortDataList> ioPortsData;
        UserEditable<nx::vms::api::IoModuleVisualStyle> visualStyle;
    };
    IoModuleSettings singleIoModuleSettings;

    camera_settings_detail::CombinedProperties devicesDescription;

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
        UserEditableMultiple<bool> recordAudioEnabled;
        UserEditableMultiple<nx::core::ptz::PresetType> preferredPtzPresetType;
        UserEditableMultiple<bool> forcedPtzPanTiltCapability;
        UserEditableMultiple<bool> forcedPtzZoomCapability;
        UserEditableMultiple<bool> doNotSendStopPtzCommand;
        UserEditableMultiple<vms::api::RtpTransportType> rtpTransportType;
        UserEditableMultiple<bool> remoteMotionDetectionEnabled;
        PtzSensitivity ptzSensitivity;

        UserEditableMultiple<int> customWebPagePort;

        static constexpr int kDefaultRtspPort = 554;
        UserEditableMultiple<int> customMediaPort;
        int customMediaPortDisplayValue = kDefaultRtspPort;
        bool areOnvifSettingsApplicable = false;
        UserEditableMultiple<bool> trustCameraTime;
        UserEditableMultiple<QString> forcedPrimaryProfile;
        UserEditableMultiple<QString> forcedSecondaryProfile;
        nx::vms::api::DeviceProfiles availableProfiles;
        UserEditableMultiple<common::RemoteArchiveSyncronizationMode>
            remoteArchiveSyncronizationMode;
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

        RecordingPeriod minPeriod = RecordingPeriod::minPeriod();
        RecordingPeriod maxPeriod = RecordingPeriod::maxPeriod();

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
    RecordingAlerts recordingAlerts;
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
        QList<core::AnalyticsEngineInfo> engines;

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
            QJsonObject errors;
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
    bool hasExportPermission = false;
    bool screenRecordingOn = false;

    // Helper methods.

    bool isSingleCamera() const;
    bool isSingleVirtualCamera() const;
    QnUuid singleCameraId() const;
    int maxRecordingBrushFps() const;
    bool isMotionDetectionEnabled() const;

    /**
     * Checks if motion detection is enabled and actually can work (hardware, or not exceeding
     * resolution limit or is forced).
     */
    bool isMotionDetectionActive() const;
    bool isObjectDetectionSupported() const;
    bool supportsRecordingByEvents() const;

    /** Metadata types for recording, based on radiobutton state. */
    static nx::vms::api::RecordingMetadataTypes radioButtonToMetadataTypes(
        MetadataRadioButton value);

    bool isDualStreamingEnabled() const;
    bool supportsSchedule() const;
    bool supportsVideoStreamControl() const;
    bool analyticsStreamSelectionEnabled(const QnUuid& engineId) const;
    bool canSwitchPtzPresetTypes() const;
    bool canForcePanTiltCapabilities() const;
    bool canForceZoomCapability() const;
    bool hasPanTiltCapabilities() const;
    bool canShowWebPage() const;
    bool canAdjustPtzSensitivity() const;
    bool cameraControlEnabled() const;
    bool canShowHotspotsPage() const;

    /**
     * Advanced settings should be displayed for a single camera when manifest is already loaded and
     * it is not empty. Most properties are available only when camera is online, but some cameras
     * may have 'Advanced' page when offline also (e.g. with 'Reboot' button).
     */
    bool canShowAdvancedPage() const;
    bool isPageVisible(CameraSettingsTab page) const;
    StreamIndex effectiveMotionStream() const;
    bool isMotionImplicitlyDisabled() const;
};

inline std::ostream& operator<<(std::ostream& os, CameraSettingsDialogState::ScheduleAlerts value)
{
    return os << nx::reflect::toString(value);
}

NX_REFLECTION_INSTRUMENT(CameraSettingsDialogState,
    (hasChanges)(singleCameraProperties)(devicesDescription)(motion))

} // namespace nx::vms::client::desktop
