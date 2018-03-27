#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

class QnAspectRatio;

namespace nx {
namespace client {
namespace desktop {

class Rotation;
struct CameraSettingsDialogState;

class CameraSettingsDialogStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsDialogStore(QObject* parent = nullptr);
    virtual ~CameraSettingsDialogStore() override;

    const CameraSettingsDialogState& state() const;

    // Actions.
    void applyChanges();
    void setReadOnly(bool value);
    void setPanicMode(bool value);
    void loadCameras(const QnVirtualCameraResourceList& cameras);
    void setSingleCameraUserName(const QString& text);
    void setScheduleBrushRecordingType(Qn::RecordingType value);
    void setScheduleBrushFps(int value);
    void setScheduleBrushQuality(Qn::StreamQuality value);
    void setRecordingShowFps(bool value);
    void setRecordingShowQuality(bool value);
    void toggleCustomBitrateVisible();
    void setCustomRecordingBitrateMbps(float mbps);
    void setCustomRecordingBitrateNormalized(float value);
    void setMinRecordingDaysAutomatic(bool value);
    void setMinRecordingDaysValue(int value);
    void setMaxRecordingDaysAutomatic(bool value);
    void setMaxRecordingDaysValue(int value);
    void setCustomAspectRatio(const QnAspectRatio& value);
    void setCustomRotation(const Rotation& value);

signals:
    void stateChanged(const CameraSettingsDialogState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
