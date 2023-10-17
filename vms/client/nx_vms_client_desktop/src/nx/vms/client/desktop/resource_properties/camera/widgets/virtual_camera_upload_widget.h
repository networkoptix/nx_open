// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_fwd.h>

namespace Ui { class VirtualCameraUploadWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class VirtualCameraUploadWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit VirtualCameraUploadWidget(QWidget* parent = nullptr);
    virtual ~VirtualCameraUploadWidget() override;

    void setStore(CameraSettingsDialogStore* store);

signals:
    void actionRequested(nx::vms::client::desktop::menu::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::VirtualCameraUploadWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
};

} // namespace nx::vms::client::desktop
