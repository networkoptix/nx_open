// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <core/resource/resource_fwd.h>

namespace Ui { class CameraSettingsGeneralTabWidget; }

class QPushButton;

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class LicenseUsageProvider;

class CameraSettingsGeneralTabWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraSettingsGeneralTabWidget(
        LicenseUsageProvider* licenseUsageProvider,
        CameraSettingsDialogStore* store,
        QWidget* parent = nullptr);

    virtual ~CameraSettingsGeneralTabWidget() override;

signals:
    void actionRequested(nx::vms::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);
    void editCredentials(CameraSettingsDialogStore* store);
    void editCameraStreams(CameraSettingsDialogStore* store);

private:
    const QScopedPointer<Ui::CameraSettingsGeneralTabWidget> ui;
};

} // namespace nx::vms::client::desktop
