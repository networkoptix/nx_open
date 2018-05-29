#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/ui/actions/actions.h>

namespace Ui { class CameraSettingsGeneralTabWidget; }

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class AbstractTextProvider;

class CameraSettingsGeneralTabWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraSettingsGeneralTabWidget(
        AbstractTextProvider* licenseUsageTextProvider,
        CameraSettingsDialogStore* store,
        QWidget* parent = nullptr);

    virtual ~CameraSettingsGeneralTabWidget() override;

signals:
    void actionRequested(nx::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::CameraSettingsGeneralTabWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
