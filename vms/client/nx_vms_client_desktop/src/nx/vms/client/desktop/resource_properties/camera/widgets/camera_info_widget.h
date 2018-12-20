#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/common/widgets/panel.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>

namespace Ui { class CameraInfoWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraInfoWidget: public Panel
{
    Q_OBJECT
    using base_type = Panel;

public:
    explicit CameraInfoWidget(QWidget* parent = nullptr);
    virtual ~CameraInfoWidget() override;

    void setStore(CameraSettingsDialogStore* store);

signals:
    void actionRequested(nx::vms::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

    void alignLabels();
    void updatePalette();
    void updatePageSwitcher();

private:
    const QScopedPointer<Ui::CameraInfoWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
};

} // namespace nx::vms::client::desktop
