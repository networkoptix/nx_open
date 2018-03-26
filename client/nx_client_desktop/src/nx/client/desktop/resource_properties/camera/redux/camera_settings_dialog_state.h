#pragma once

#include <core/resource/device_dependent_strings.h>
#include <ui/widgets/properties/schedule_grid_widget.h>
#include <core/resource/media_stream_capability.h>
#include <nx_ec/data/api_camera_attributes_data.h>

#include <nx/client/desktop/common/data/rotation.h>

#include <nx/utils/std/optional.h>
#include <utils/common/aspect_ratio.h>

namespace nx {
namespace client {
namespace desktop {

template<class T>
struct UserEditable
{
    T get() const { return m_user.value_or(m_base); }
    void setUser(T value) { m_user = value; }
    void setBase(T value) {m_base = value;}
    void resetUser() { m_user.reset(); }

    T operator()() const { return get(); }

private:
    T m_base;
    std::optional<T> m_user;
};

template<class T>
struct UserEditableMultiple
{
    bool hasValue() const { return m_user.has_value() || m_base.has_value(); }
    T get() const { return m_user.value_or(m_base.value()); }
    void setUser(T value) { m_user = value; }
    void setBase(T value) {m_base = value;}
    void resetUser() { m_user.reset(); }
    void resetBase() { m_base.reset(); }

    T operator()() const { return get(); }

private:
    std::optional<T> m_base;
    std::optional<T> m_user;
};


struct CameraSettingsDialogState
{
    CameraSettingsDialogState() = default;
    CameraSettingsDialogState(const CameraSettingsDialogState& other) = delete;
    CameraSettingsDialogState(CameraSettingsDialogState&& other) = default;
    CameraSettingsDialogState& operator=(const CameraSettingsDialogState&) = delete;
    CameraSettingsDialogState& operator=(CameraSettingsDialogState&&) = default;
    ~CameraSettingsDialogState() = default;

    bool hasChanges = false;

    // Generic cameras info.

    int devicesCount = 0;
    QnCameraDeviceType deviceType = QnCameraDeviceType::Mixed;

    struct SingleCameraSettings
    {
        QString id;
        UserEditable<QString> name;
        QString firmware;
        QString model;
        QString vendor;
        QString macAddress;
        QString ipAddress;
        QString webPage;
        QString primaryStream;
        QString secondaryStream;
    };
    SingleCameraSettings singleCameraSettings;

    struct RecordingDays
    {
        int absoluteValue = 0;
        bool automatic = true;
        bool same = true;
    };

    struct RecordingSettings
    {
        // TODO: #GDM Refactor QnScheduleGridWidget.
        QnScheduleGridWidget::CellParams brush;
        int maxFps = 0;
        int maxDualStreamingFps = 0;
        int maxBrushFps() const
        {
            return brush.recordingType == Qn::RT_MotionAndLowQuality
                ? maxDualStreamingFps
                : maxFps;
        }

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

        bool showQuality = true;
        bool showFps = true;

        RecordingDays minDays{ec2::kDefaultMinArchiveDays, true, true};
        RecordingDays maxDays{ec2::kDefaultMaxArchiveDays, true, true};

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
};

} // namespace desktop
} // namespace client
} // namespace nx