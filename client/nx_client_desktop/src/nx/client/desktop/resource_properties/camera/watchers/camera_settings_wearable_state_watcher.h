#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/utils/wearable_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsDialogStore;

class CameraSettingsWearableStateWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsWearableStateWatcher(CameraSettingsDialogStore* store,
        QObject* parent = nullptr);

    void setCameras(const QnVirtualCameraResourceList& cameras);

signals:
    void wearableStateChanged(const nx::client::desktop::WearableState& state, QPrivateSignal);

private:
    void updateState();

private:
    QnVirtualCameraResourcePtr m_camera;
};

} // namespace desktop
} // namespace client
} // namespace nx
