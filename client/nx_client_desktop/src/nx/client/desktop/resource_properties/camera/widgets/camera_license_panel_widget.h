#pragma once

#include <QtCore/QScopedPointer>

#include <nx/client/desktop/common/widgets/panel.h>
#include <nx/client/desktop/ui/actions/actions.h>

class QnDisconnectHelper;
namespace Ui { class CameraLicensePanelWidget; }

namespace nx {
namespace client {
namespace desktop {

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
    void actionRequested(nx::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::CameraLicensePanelWidget> ui;
    const QScopedPointer<ProvidedTextDisplay> m_licenseUsageDisplay;
    QScopedPointer<QnDisconnectHelper> m_storeConnections;
};

} // namespace desktop
} // namespace client
} // namespace nx
