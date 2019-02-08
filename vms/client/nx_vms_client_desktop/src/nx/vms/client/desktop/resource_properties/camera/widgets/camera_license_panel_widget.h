#pragma once

#include <QtCore/QScopedPointer>

#include <nx/vms/client/desktop/common/widgets/panel.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/utils/scoped_connections.h>

namespace Ui { class CameraLicensePanelWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class AbstractTextProvider;
class ProvidedTextDisplay;

class CameraLicensePanelWidget: public Panel
{
    Q_OBJECT
    using base_type = Panel;

public:
    explicit CameraLicensePanelWidget(QWidget* parent = nullptr);
    virtual ~CameraLicensePanelWidget() override;

    void init(AbstractTextProvider* licenseUsageTextProvider, CameraSettingsDialogStore* store);

signals:
    void actionRequested(nx::vms::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::CameraLicensePanelWidget> ui;
    const QScopedPointer<ProvidedTextDisplay> m_licenseUsageDisplay;
    nx::utils::ScopedConnections m_storeConnections;
};

} // namespace nx::vms::client::desktop
