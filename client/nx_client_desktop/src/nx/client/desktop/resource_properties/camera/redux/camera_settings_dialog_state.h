#pragma once

#include <QtCore/QList>

#include <core/ptz/media_dewarping_params.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_stream_capability.h>
#include <core/resource/motion_window.h>
#include <core/misc/schedule_task.h>

#include <nx/vms/api/data/camera_attributes_data.h>

#include <nx/client/desktop/common/data/rotation.h>
#include <nx/client/desktop/resource_properties/camera/utils/schedule_cell_params.h>

#include <utils/common/aspect_ratio.h>

#include <nx/utils/std/optional.h>

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
        void resetUser() { m_user.reset(); }

        T operator()() const { return get(); }

        void updateValue(T value)
        {
            if (m_user.has_value() || m_base != value)
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
        void resetUser() { m_user.reset(); }
        void resetBase() { m_base.reset(); }

        T operator()() const { return get(); }

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
        BrushChanged,

        // Recording was enabled, while schedule is empty.
        EmptySchedule,

        // Not enough licenses to enable recording.
        NotEnoughLicenses,

        // License limit exceeded, recording will not be enabled.
        LicenseLimitExceeded,

        // Schedule was changed but recording is not enabled.
        RecordingIsNotEnabled,

        // High minimal archive length value selected.
        HighArchiveLength,

        // Motion detection was enabled while recording was not.
        MotionDetectionRequiresRecording,

        // Selection attempt produced too many motion rectangles.
        MotionDetectionTooManyRectangles,

        // Selection attempt produced too many motion mask rectangles.
        MotionDetectionTooManyMaskRectangles,

        // Selection attempt produced too many motion sensitivity rectangles.
        MotionDetectionTooManySensitivityRectangles,
    };

    CameraSettingsDialogState() = default;
    CameraSettingsDialogState(const CameraSettingsDialogState& other) = delete;
    CameraSettingsDialogState(CameraSettingsDialogState&& other) = default;
    CameraSettingsDialogState& operator=(const CameraSettingsDialogState&) = delete;
    CameraSettingsDialogState& operator=(CameraSettingsDialogState&&) = default;
    ~CameraSettingsDialogState() = default;

    bool hasChanges = false;
    bool readOnly = true;
    bool panicMode = false;

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
        std::optional<QString> primaryStream;
        std::optional<QString> secondaryStream;

        int maxFpsWithoutMotion = 0;

        std::optional<MotionConstraints> motionConstraints;
    };
    SingleCameraProperties singleCameraProperties;

    struct FisheyeCalibrationSettings
    {
        QPointF offset;
        qreal radius = 0.5;
        qreal aspectRatio = 1.0;

        bool operator==(const FisheyeCalibrationSettings& s) const
        {
            return offset == s.offset && radius == s.radius && aspectRatio == s.aspectRatio;
        }

        bool operator!=(const FisheyeCalibrationSettings& s) const
        {
            return offset != s.offset || radius != s.radius && aspectRatio != s.aspectRatio;
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
    };
    SingleCameraSettings singleCameraSettings;

    struct CombinedProperties
    {
        CombinedValue isDtsBased;
        CombinedValue isWearable;
        CombinedValue hasMotion;
        CombinedValue hasDualStreaming;

        int maxFps = 0;
        int maxDualStreamingFps = 0;
    };
    CombinedProperties devicesDescription;

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

    std::optional<Alert> alert;

    struct ImageControlSettings
    {
        bool aspectRatioAvailable = true;
        UserEditableMultiple<QnAspectRatio> aspectRatio;
        bool rotationAvailable = true;
        UserEditableMultiple<Rotation> rotation;
    };
    ImageControlSettings imageControl;

    // Helper methods.

    bool isSingleCamera() const { return devicesCount == 1; }

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

    bool hasDualStreaming() const
    {
        return hasMotion() && devicesDescription.hasDualStreaming == CombinedValue::All;
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
        return params;
    }
};

} // namespace desktop
} // namespace client
} // namespace nx
