#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsReadOnlyWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsReadOnlyWatcher(CameraSettingsDialogStore* store,
        QObject* parent = nullptr);

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList& value);

    bool isReadOnly() const;

signals:
    void readOnlyChanged(bool value, QPrivateSignal);

private:
    void updateReadOnly();
    bool calculateReadOnly() const;

private:
    bool m_readOnly = true;
    QnVirtualCameraResourceList m_cameras;
};

} // namespace nx::vms::client::desktop
