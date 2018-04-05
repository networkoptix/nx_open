#pragma once

#include <nx/client/desktop/common/widgets/panel.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

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

private:
    void loadState(const CameraSettingsDialogState& state);

    void alignLabels();
    void updatePalette();
    void updatePageSwitcher();

private:
    QScopedPointer<Ui::CameraInfoWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
