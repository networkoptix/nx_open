#pragma once

#include <QtCore/QList>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/motion_window.h>
#include <core/misc/schedule_task.h>
#include <utils/common/aspect_ratio.h>

#include <nx/client/desktop/common/data/rotation.h>
#include <nx/client/desktop/resource_properties/camera/utils/schedule_cell_params.h>
#include <nx/client/desktop/utils/wearable_state.h>
#include <nx/utils/std/optional.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState
{
    template<class T>
    struct UserEditable
    {
        T get() const { return m_user.value_or(m_base); }
        void setUser(T value) { m_user = value; }
        void setBase(T value) { m_base = value; }
        void resetUser() { m_user = {}; }

        T operator()() const { return get(); }

        void setUserIfChanged(T value)
        {
            if (m_user || m_base != value)
                m_user = value;
        }

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

    enum class Alert
    {
        // Brush was changed (mode, fps, quality).
        brushChanged,

        // Recording was enabled, while schedule is empty.
        emptySchedule,

        // Not enough licenses to enable recording.
        notEnoughLicenses,

        // License limit exceeded, recording will not be enabled.
        licenseLimitExceeded,

        // Schedule was changed but recording is not enabled.
        recordingIsNotEnabled,

        // High minimal archive length value selected.
        highArchiveLength,

        // Motion detection was enabled while recording was not.
        motionDetectionRequiresRecording,

        // Selection attempt produced too many motion rectangles.
        motionDetectionTooManyRectangles,

        // Selection attempt produced too many motion mask rectangles.
        motionDetectionTooManyMaskRectangles,

        // Selection attempt produced too many motion sensitivity rectangles.
        motionDetectionTooManySensitivityRectangles,
    };

    CameraSettingsDialogState() = default;
    CameraSettingsDialogState(const CameraSettingsDialogState& other) = delete;
    CameraSettingsDialogState(CameraSettingsDialogState&& other) = default;
    CameraSettingsDialogState& operator=(const CameraSettingsDialogState&) = delete;
    CameraSettingsDialogState& operator=(CameraSettingsDialogState&&) = default;
    ~CameraSettingsDialogState() = default;

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

    struct FisheyeCalibrationSettings
    {
        QPointF offset;
        qreal radius = 0.5;
        qreal aspectRatio = 1.0;

        bool operator==(const FisheyeCalibrationSettings& s) const
        {
            return offset == s.offset && qFuzzyEquals(radius, s.radius)
                && qFuzzyEquals(aspectRatio, s.aspectRatio);
        }

        bool operator!=(const FisheyeCalibrationSettings& s) const
        {
            return !(*this == s);
        }
    };

    struct SingleCameraSettings
    {
        UserEditable<bool> enableMotionDetection;
        UserEditable<QList<QnMotionRegion>> motionRegionList;

        UserEditable<bool> enableFisheyeDewarping;
        UserEditable<QnMediaDewarpingParams::ViewMode> fisheyeMountingType;
        UserEditable<FisheyeCalibrationSettings> fisheyeCalibrationSettings;
        UserEditable<qreal> fisheyeFovRotation;

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
        CombinedValue hasMotion;
        CombinedValue hasDualStreamingCapability;
        CombinedValue hasRemoteArchiveCapability;
        CombinedValue hasPredefinedBitratePerGOP;
        CombinedValue hasPtzPresets;
        CombinedValue canDisableNativePtzPresets;
        CombinedValue supportsMotionStreamOverride;

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
        UserEditableMultiple<bool> nativePtzPresetsDisabled;
        UserEditableMultiple<vms::api::RtpTransportType> rtpTransportType;
        UserEditableMultiple<vms::api::MotionStreamType> motionStreamType;
        CombinedValue motionStreamOverridden; //< Read-only informational value.
    };
    ExpertSettings expert;
    bool isDefaultExpertSettings = false;

    struct RecordingDays
    {
        int absoluteValue = 0;
        bool automatic = true;
        bool same = true;
    };

    struct RecordingSettings
    {
        UserEditableMultiple<bool> enabled;

        ScheduleCellParams brush;

        media::CameraStreamCapability mediaStreamCapability;
        Qn::BitratePerGopType bitratePerGopType = Qn::BPG_None;
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

        UserEditableMultiple<ScheduleTasks> schedule;

        bool showQuality = true;
        bool showFps = true;

        RecordingDays minDays{nx::vms::api::kDefaultMinArchiveDays, true, true};
        RecordingDays maxDays{nx::vms::api::kDefaultMaxArchiveDays, true, true};

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

    std::optional<Alert> alert;

    struct ImageControlSettings
    {
        bool aspectRatioAvailable = true;
        UserEditableMultiple<QnAspectRatio> aspectRatio;
        bool rotationAvailable = true;
        UserEditableMultiple<Rotation> rotation;
    };
    ImageControlSettings imageControl;

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
            && devicesDescription.isWearable == CombinedValue::None;
    }

    QnMediaDewarpingParams fisheyeSettings() const
    {
        QnMediaDewarpingParams params;
        FisheyeCalibrationSettings calibration = singleCameraSettings.fisheyeCalibrationSettings();
        params.enabled = singleCameraSettings.enableFisheyeDewarping();
        params.xCenter = 0.5 + calibration.offset.x();
        params.yCenter = 0.5 + calibration.offset.y();
        params.radius = calibration.radius;
        params.hStretch = calibration.aspectRatio;
        params.fovRot = singleCameraSettings.fisheyeFovRotation();
        params.viewMode = singleCameraSettings.fisheyeMountingType();
        return params;
    }
};

} // namespace desktop
} // namespace client
} // namespace nx
