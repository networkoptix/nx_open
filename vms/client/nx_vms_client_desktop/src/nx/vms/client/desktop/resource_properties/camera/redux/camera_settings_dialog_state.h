#pragma once

#include <QtCore/QList>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/ptz_preset.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/motion_window.h>
#include <core/misc/schedule_task.h>
#include <utils/common/aspect_ratio.h>

#include <nx/vms/client/desktop/common/data/rotation.h>
#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/schedule_cell_params.h>
#include <nx/vms/client/desktop/utils/wearable_state.h>
#include <nx/utils/std/optional.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types_fwd.h>

#include "../utils/analytics_engine_info.h"

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogState: AbstractReduxState
{
    template<class T>
    struct UserEditable
    {
        T get() const { return m_user.value_or(m_base); }
        bool hasUser() const { return m_user.has_value(); }
        void setUser(T value) { m_user = value; }
        T getBase() const { return m_base; }
        void setBase(T value) { m_base = value; }
        void resetUser() { m_user = {}; }

        T operator()() const { return get(); }

    private:
        T m_base = T();
        std::optional<T> m_user;
    };

    template<class T>
    struct UserEditableMultiple
    {
        bool hasValue() const { return m_user || m_base; }
        T get() const { return m_user.value_or(*m_base); }
        T valueOr(T value) const { return m_user.value_or(m_base.value_or(value)); }
        void setUser(T value) { m_user = value; }
        void setBase(T value) { m_base = value; }
        void resetUser() { m_user = {}; }
        void resetBase() { m_base = {}; }

        T operator()() const { return get(); }
        operator std::optional<T>() const { return m_user ? m_user : m_base; }

    private:
        std::optional<T> m_base;
        std::optional<T> m_user;
    };

    enum class CombinedValue
    {
        None,
        Some,
        All
    };

    enum class RecordingHint
    {
        // Brush was changed (mode, fps, quality).
        brushChanged,

        // Recording was enabled, while schedule is empty.
        emptySchedule,

        // Schedule was changed but recording is not enabled.
        recordingIsNotEnabled
    };

    enum class RecordingAlert
    {
        // High minimal archive length value selected.
        highArchiveLength
    };

    enum class MotionAlert
    {
        // Selection attempt produced too many motion rectangles.
        motionDetectionTooManyRectangles,

        // Selection attempt produced too many motion mask rectangles.
        motionDetectionTooManyMaskRectangles,

        // Selection attempt produced too many motion sensitivity rectangles.
        motionDetectionTooManySensitivityRectangles
    };

    bool hasChanges = false;
    bool readOnly = true;
    bool settingsOptimizationEnabled = false;
    GlobalPermissions globalPermissions;

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
        std::optional<QString> primaryStream;
        std::optional<QString> secondaryStream;
        bool hasVideo = true;

        int maxFpsWithoutMotion = 0;

        std::optional<MotionConstraints> motionConstraints;
    };
    SingleCameraProperties singleCameraProperties;

    WearableState singleWearableState;
    QString wearableUploaderName; //< Name of user currently uploading footage to wearable camera.

    struct SingleCameraSettings
    {
        UserEditable<bool> enableMotionDetection;
        UserEditable<QList<QnMotionRegion>> motionRegionList;

        UserEditable<QnMediaDewarpingParams> fisheyeDewarping;

        UserEditable<int> logicalId;
        QStringList sameLogicalIdCameraNames; //< Read-only informational value.
    };
    SingleCameraSettings singleCameraSettings;

    struct IoModuleSettings
    {
        UserEditable<QnIOPortDataList> ioPortsData;
        UserEditable<vms::api::IoModuleVisualStyle> visualStyle;
    };
    IoModuleSettings singleIoModuleSettings;

    struct CombinedProperties
    {
        CombinedValue isDtsBased;
        CombinedValue isWearable;
        CombinedValue isIoModule;
        CombinedValue isArecontCamera;
        CombinedValue supportsAudio;
        CombinedValue supportsVideo;
        CombinedValue isAudioForced;
        CombinedValue hasMotion;
        CombinedValue hasDualStreamingCapability;
        CombinedValue hasRemoteArchiveCapability;
        CombinedValue canSwitchPtzPresetTypes;
        CombinedValue canForcePtzCapabilities;
        CombinedValue supportsMotionStreamOverride;
        CombinedValue hasCustomMediaPortCapability;
        CombinedValue supportsRecording;

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
        UserEditableMultiple<vms::api::RtpTransportType> rtpTransportType;
        UserEditableMultiple<vms::api::MotionStreamType> motionStreamType;
        CombinedValue motionStreamOverridden; //< Read-only informational value.

        static constexpr int kDefaultRtspPort = 554;
        UserEditableMultiple<int> customMediaPort;
        int customMediaPortDisplayValue = kDefaultRtspPort;
        UserEditableMultiple<bool> trustCameraTime;
    };
    ExpertSettings expert;
    bool isDefaultExpertSettings = false;

    struct RecordingDays
    {
        UserEditableMultiple<bool> automatic;
        UserEditableMultiple<int> value;
    };

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

        RecordingDays minDays;
        RecordingDays maxDays;

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
        QList<AnalyticsEngineInfo> engines;
        UserEditable<QSet<QnUuid>> enabledEngines;
        QHash<QnUuid, UserEditable<QVariantMap>> settingsValuesByEngineId;
        bool loading = false;
        QnUuid currentEngineId;
    };
    AnalyticsSettings analytics;

    struct WearableCameraMotionDetection
    {
        UserEditableMultiple<bool> enabled;
        UserEditableMultiple<int> sensitivity;
    };
    WearableCameraMotionDetection wearableMotion;

    // Helper methods.

    bool isSingleCamera() const { return devicesCount == 1; }

    bool isSingleWearableCamera() const
    {
        return isSingleCamera() && devicesDescription.isWearable == CombinedValue::All;
    }

    int maxRecordingBrushFps() const
    {
        if (isSingleCamera() && !singleCameraSettings.enableMotionDetection())
            return singleCameraProperties.maxFpsWithoutMotion;

        return recording.brush.recordingType == Qn::RecordingType::motionAndLow
            ? devicesDescription.maxDualStreamingFps
            : devicesDescription.maxFps;
    }

    bool hasMotion() const
    {
        bool result = devicesDescription.hasMotion == CombinedValue::All;
        if (isSingleCamera())
            result &= singleCameraSettings.enableMotionDetection();
        return result;
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

    bool canSwitchPtzPresetTypes() const
    {
        return devicesDescription.canSwitchPtzPresetTypes != CombinedValue::None;
    }

    bool canForcePtzCapabilities() const
    {
        return devicesDescription.canForcePtzCapabilities == CombinedValue::All;
    }
};

} // namespace nx::vms::client::desktop
