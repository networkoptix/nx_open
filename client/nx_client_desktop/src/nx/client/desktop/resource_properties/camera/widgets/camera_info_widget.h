#pragma once

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <nx/client/desktop/common/widgets/panel.h>

class QnDisconnectHelper;
namespace Ui { class CameraInfoWidget; }

namespace nx {
namespace client {
namespace desktop {

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
    void requestPing();
    void requestShowOnLayout();
    void requestEventLog();
    void requestEventRules();

private:
    void loadState(const CameraSettingsDialogState& state);

    void alignLabels();
    void updatePalette();
    void updatePageSwitcher();

private:
    QScopedPointer<Ui::CameraInfoWidget> ui;
    QScopedPointer<QnDisconnectHelper> m_storeConnections;
};

} // namespace desktop
} // namespace client
} // namespace nx
