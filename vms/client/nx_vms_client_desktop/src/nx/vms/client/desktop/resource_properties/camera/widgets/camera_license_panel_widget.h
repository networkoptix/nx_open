// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/common/widgets/panel.h>
#include <nx/vms/client/desktop/menu/actions.h>

namespace Ui { class CameraLicensePanelWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class LicenseUsageProvider;
class ProvidedTextDisplay;

class CameraLicensePanelWidget: public Panel
{
    Q_OBJECT
    using base_type = Panel;

public:
    explicit CameraLicensePanelWidget(QWidget* parent = nullptr);
    virtual ~CameraLicensePanelWidget() override;

    void init(LicenseUsageProvider* licenseUsageProvider, CameraSettingsDialogStore* store);

signals:
    void actionRequested(nx::vms::client::desktop::menu::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);
    void updateLicensesButton(const CameraSettingsDialogState& state);

private:
    nx::utils::ImplPtr<Ui::CameraLicensePanelWidget> ui;
    nx::utils::ImplPtr<ProvidedTextDisplay> m_licenseUsageDisplay;
    nx::utils::ScopedConnections m_storeConnections;
    QPointer<LicenseUsageProvider> m_licenseUsageProvider;
};

} // namespace nx::vms::client::desktop
