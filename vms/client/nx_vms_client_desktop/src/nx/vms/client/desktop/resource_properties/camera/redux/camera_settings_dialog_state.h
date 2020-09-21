#pragma once

#include <optional>

#include <QtCore/QList>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/ptz_preset.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/motion_window.h>
#include <core/misc/schedule_task.h>
#include <utils/common/aspect_ratio.h>

#include <nx/vms/client/desktop/analytics/analytics_settings_types.h>
#include <nx/vms/client/desktop/common/data/rotation.h>
#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>
#include <nx/vms/client/desktop/common/redux/redux_types.h>
#include <nx/vms/client/desktop/utils/wearable_state.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types_fwd.h>

#include "../data/recording_days.h"
#include "../data/schedule_cell_params.h"
#include "../data/analytics_engine_info.h"
#include "../camera_settings_tab.h"

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogState: AbstractReduxState
{
    using StreamIndex = nx::vms::api::StreamIndex;

    enum class RecordingHint
    {
        // Brush was changed (mode, fps, quality).
        brushChanged,

        // Recording was enabled, while schedule is empty.
        emptySchedule,

        // Schedule was changed but recording is not enabled.
        recordingIsNotEnabled,
    };

    enum class RecordingAlert
    {
        // High minimal archive length value selected.
        highArchiveLength,

        // High pre-recording value selected, resulting in increased RAM usage on the server.
        highPreRecordingValue,
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

    enum class ScheduleAlert
    {
        // "Motion + LQ" schedule records will be changed to "Always" due to no dual streaming.
        scheduleChangeDueToNoDualStreaming,

        // "Motion" and "Motion + LQ" schedule records will be changed to "Always" due to no motion.
        scheduleChangeDueToNoMotion,
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
        QString id;
        UserEditable<QString> name;
        QString firmware;
        QString model;
        QString vendor;
        QString macAddress;
        QString ipAddress;
        QString webPage;
        QString settingsUrlPath;
        int overrideXmlHttpRequestTimeout = 0;
        QString overrideHttpUserAgent;
        bool isOnline = false;
        bool fixupRequestUrls = false;
        bool hasVideo = true;
        bool editableStreamUrls = false;
        bool networkLink = false;
        bool usbDevice = false;

        int maxFpsWithoutMotion = 0;

        std::optional<MotionConstraints> motionConstraints;
        std::optional<QnCameraAdvancedParams> advancedSettingsManifest;
    };
    SingleCameraProperties singleCameraProperties;

    WearableState singleWearableState;
    QString wearableUploaderName; //< Name of user currently uploading footage to wearable camera.

    struct SingleCameraSettings
    {
        UserEditable<QList<QnMotionRegion>> motionRegionList;

        UserEditable<QnMediaDewarpingParams> fisheyeDewarping;

        UserEditable<QString> primaryStream;
        UserEditable<QString> secondaryStream;

        UserEditable<int> logicalId;
        QStringList sameLogicalIdCameraNames; //< Read-only informational value.
    };
    SingleCameraSettings singleCameraSettings;

    UserEditableMultiple<bool> enableMotionDetection;

    struct IoModuleSettings
    {
        UserEditable<QnIOPortDataList> ioPortsData;
        UserEditable<nx::vms::api::IoModuleVisualStyle> visualStyle;
    };
    IoModuleSettings singleIoModuleSettings;

    struct CombinedProperties
    {
        CombinedValue isDtsBased = CombinedValue::None;
        CombinedValue isWearable = CombinedValue::None;
        CombinedValue isIoModule = CombinedValue::None;
        CombinedValue isArecontCamera = CombinedValue::None;
        CombinedValue supportsAudio = CombinedValue::None;
        CombinedValue supportsVideo = CombinedValue::None;
        CombinedValue isAudioForced = CombinedValue::None;
        CombinedValue hasMotion = CombinedValue::None;
        CombinedValue hasDualStreamingCapability = CombinedValue::None;
        CombinedValue hasRemoteArchiveCapability = CombinedValue::None;
        CombinedValue canSwitchPtzPresetTypes = CombinedValue::None;
        CombinedValue canForcePanTiltCapabilities = CombinedValue::None;
        CombinedValue canForceZoomCapability = CombinedValue::None;
        CombinedValue supportsMotionStreamOverride = CombinedValue::None;
        CombinedValue hasCustomMediaPortCapability = CombinedValue::None;
        CombinedValue supportsRecording = CombinedValue::None;
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

    struct ExpertSettings
    {
        UserEditableMultiple<bool> dualStreamingDisabled;
        UserEditableMultiple<bool> cameraControlDisabled;
        UserEditableMultiple<bool> useBitratePerGOP;
        UserEditableMultiple<bool> primaryRecordingDisabled;
        UserEditableMultiple<bool> secondaryRecordingDisabled;
        UserEditableMultiple<nx::core::ptz::PresetType> preferredPtzPresetType;
        UserEditableMultiple<bool> forcedPtzPanTiltCapability;
        UserEditableMultiple<bool> forcedPtzZoomCapability;
        UserEditableMultiple<nx::vms::api::RtpTransportType> rtpTransportType;
        UserEditableMultiple<StreamIndex> forcedMotionStreamType;
        CombinedValue motionStreamOverridden = CombinedValue::None;
        UserEditableMultiple<bool> remoteMotionDetectionEnabled;

        static constexpr int kDefaultRtspPort = 554;
        UserEditableMultiple<int> customMediaPort;
        int customMediaPortDisplayValue = kDefaultRtspPort;
        UserEditableMultiple<bool> trustCameraTime;
    };
    ExpertSettings expert;
    bool isDefaultExpertSettings = false;

    struct RecordingSettings
    {
        UserEditableMultiple<bool> enabled;

        ScheduleCellParams brush;

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

        bool isCustomBitrate() const
        {
            return !brush.isAutomaticBitrate();
        }

        float normalizedCustomBitrateMbps() const
        {
            const auto spread = maxBitrateMpbs - minBitrateMbps;
            if (qFuzzyIsNull(spread))
                return minBitrateMbps;
            return (bitrateMbps - minBitrateMbps) / spread;
        }

    };
    RecordingSettings recording;

    UserEditableMultiple<bool> audioEnabled;

    std::optional<RecordingHint> recordingHint;
    std::optional<RecordingAlert> recordingAlert;
    std::optional<MotionAlert> motionAlert;
    std::optional<ScheduleAlert> scheduleAlert;

    struct ImageControlSettings
    {
        bool aspectRatioAvailable = true;
        UserEditableMultiple<QnAspectRatio> aspectRatio;
        bool rotationAvailable = true;
        UserEditableMultiple<Rotation> rotation;
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

    struct WearableCameraMotionDetection
    {
        UserEditableMultiple<bool> enabled;
        UserEditableMultiple<int> sensitivity;
    };
    WearableCameraMotionDetection wearableMotion;

    bool wearableIgnoreTimeZone = false;

    // Helper methods.

    bool isSingleCamera() const { return devicesCount == 1; }

    bool isSingleWearableCamera() const
    {
        return isSingleCamera() && devicesDescription.isWearable == CombinedValue::All;
    }

    int maxRecordingBrushFps() const
    {
        if (isSingleCamera() && !isMotionDetectionEnabled())
            return singleCameraProperties.maxFpsWithoutMotion;

        return recording.brush.recordingType == Qn::RecordingType::motionAndLow
            ? devicesDescription.maxDualStreamingFps
            : devicesDescription.maxFps;
    }

    bool isMotionDetectionStreamEnabled() const
    {
        return !settingsOptimizationEnabled || expert.dualStreamingDisabled.equals(false)
            || expert.forcedMotionStreamType.equals(StreamIndex::primary);
    }

    bool isMotionDetectionEnabled() const
    {
        return enableMotionDetection.equals(true) && isMotionDetectionStreamEnabled();
    }

    bool isDualStreamingEnabled() const
    {
        return devicesDescription.hasDualStreamingCapability == CombinedValue::All
            && (!settingsOptimizationEnabled || expert.dualStreamingDisabled.equals(false));
    }

    bool supportsMotionPlusLQ() const
    {
        return isMotionDetectionEnabled() && isDualStreamingEnabled();
    }

    bool supportsSchedule() const
    {
        return devicesDescription.isDtsBased == CombinedValue::None
            && devicesDescription.isWearable == CombinedValue::None
            && devicesDescription.supportsRecording == CombinedValue::All;
    }

    bool supportsVideoStreamControl() const
    {
        return devicesDescription.isWearable == CombinedValue::None
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
            && devicesDescription.isWearable == CombinedValue::None;
    }

    /**
     * Advanced settings should be displayed for a single camera when manifest is already loaded and
     * it is not empty. Most properties are avalailable only when camera is online, but some cameras
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
                    && devicesDescription.isWearable == CombinedValue::None
                    && devicesDescription.isIoModule == CombinedValue::All;

            case CameraSettingsTab::motion:
                return isSingleCamera()
                    && devicesDescription.isWearable == CombinedValue::None
                    && devicesDescription.isDtsBased == CombinedValue::None
                    && devicesDescription.supportsVideo == CombinedValue::All
                    && devicesDescription.hasMotion == CombinedValue::All;

            case CameraSettingsTab::fisheye:
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

} // namespace nx::vms::client::desktop
