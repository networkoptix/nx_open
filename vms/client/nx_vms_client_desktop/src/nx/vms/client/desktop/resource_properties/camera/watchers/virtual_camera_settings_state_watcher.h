// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_fwd.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class VirtualCameraSettingsStateWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit VirtualCameraSettingsStateWatcher(CameraSettingsDialogStore* store,
        QObject* parent = nullptr);

    void setCameras(const QnVirtualCameraResourceList& cameras);

signals:
    void virtualCameraStateChanged(const nx::vms::client::desktop::VirtualCameraState& state, QPrivateSignal);

private:
    void updateState();

private:
    QnVirtualCameraResourcePtr m_camera;
    nx::utils::ScopedConnection m_managerConnection;
};

} // namespace nx::vms::client::desktop
