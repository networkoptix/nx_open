// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/scoped_connections.h>

namespace Ui { class VirtualCameraTimeZoneWidget; }

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;
struct CameraSettingsDialogState;

class VirtualCameraTimeZoneWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit VirtualCameraTimeZoneWidget(QWidget* parent = nullptr);
    virtual ~VirtualCameraTimeZoneWidget() override;

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::VirtualCameraTimeZoneWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
};

} // namespace nx::vms::client::desktop
